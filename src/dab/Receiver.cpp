/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Receiver.h"

#include "dab/EnsembleReader.h"
#include "dab/SignalQuality.h"
#include "device/SoftwareAgc.h"
#include "utils/Log.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <utility>

#include <basic_radio/basic_audio_channel.h>
#include <basic_radio/basic_dab_plus_channel.h>
#include <basic_radio/basic_data_packet_channel.h>
#include <basic_radio/basic_radio.h>
#include <basic_radio/basic_slideshow.h>
#include <dab/constants/dab_parameters.h>
#include <dab/database/dab_database.h>
#include <dab/mot/MOT_entities.h>
#include <ofdm/ofdm_helpers.h>

namespace DABPLUS
{

namespace
{

constexpr int TRANSMISSION_MODE = 1;
constexpr int DEMODULATOR_THREADS = 1;
constexpr size_t DECODER_THREADS = 1;

// Each about one second of signal
constexpr size_t MAX_QUEUED_SAMPLE_BLOCKS = 64;
constexpr size_t MAX_QUEUED_FRAMES = 10;

// Samples of the previous frequency may still be buffered by the source, e.g. in the network
constexpr std::chrono::milliseconds DISCARD_AFTER_TUNE{300};

// Tuners like the R820T sometimes fail to lock to a frequency; setting it again helps
constexpr std::chrono::seconds RETUNE_WITHOUT_SIGNAL{2};

// ETSI TS 101 756, table 17
constexpr uint8_t MOT_CONTENT_TYPE_IMAGE = 2;
constexpr uint16_t MOT_CONTENT_SUBTYPE_JPEG = 1;
constexpr uint16_t MOT_CONTENT_SUBTYPE_PNG = 3;

// Of the signal quality, per frame or sample block
constexpr float SMOOTHING = 0.2f;
// The modulation error ratio is calculated from every n-th OFDM symbol only
constexpr size_t MER_SYMBOL_STEP = 4;

float Smooth(float average, float value)
{
  return average + SMOOTHING * (value - average);
}

float CalculateFrameMer(const OFDM_Demod& demodulator)
{
  const OFDM_Params params = demodulator.GetOFDMParams();
  const auto carriers = demodulator.GetFrameDataVec();
  const size_t carriersPerSymbol = params.nb_data_carriers;
  const size_t symbols = params.nb_frame_symbols - 1;

  std::vector<std::complex<float>> sampled;
  sampled.reserve(symbols / MER_SYMBOL_STEP * carriersPerSymbol + carriersPerSymbol);
  for (size_t symbol = 0; symbol < symbols; symbol += MER_SYMBOL_STEP)
  {
    const auto symbolCarriers = carriers.subspan(symbol * carriersPerSymbol, carriersPerSymbol);
    sampled.insert(sampled.end(), symbolCarriers.begin(), symbolCarriers.end());
  }
  return CalculateMer(sampled);
}

constexpr auto USER_APPLICATION_SPI =
    static_cast<user_application_type_t>(UserApplicationType::SPI);

bool IsSpiComponent(const ServiceComponent& component)
{
  return component.transport_mode == TransportMode::PACKET_MODE_DATA &&
         std::ranges::find(component.application_types, USER_APPLICATION_SPI) !=
             component.application_types.end();
}

int FindNearestGain(const std::vector<int>& gains, int gain)
{
  return *std::ranges::min_element(gains, {}, [gain](int g) { return std::abs(g - gain); });
}

} // unnamed namespace

CReceiver::CReceiver(std::unique_ptr<ISampleSource> source, GainConfig gain)
  : m_source(std::move(source)),
    m_gainConfig(gain)
{
}

CReceiver::~CReceiver()
{
  Stop();
}

bool CReceiver::Start()
{
  if (m_isStarted)
    return true;

  if (!m_source->Open())
    return false;

  const std::vector<int> gains = m_source->GetGains();
  if (!gains.empty())
  {
    if (m_gainConfig.automatic)
    {
      m_agc = std::make_unique<CSoftwareAgc>(gains);
      m_gain = m_agc->GetGain();
    }
    else
    {
      m_gain = FindNearestGain(gains, m_gainConfig.manualGain);
    }
    m_source->SetGain(m_gain);
  }

  m_stop = false;
  m_demodulatorThread = std::thread(&CReceiver::DemodulateSamples, this);
  m_decoderThread = std::thread(&CReceiver::DecodeFrames, this);

  if (!m_source->Start([this](std::span<const uint8_t> samples) { OnSamples(samples); }))
  {
    Stop();
    return false;
  }

  m_isStarted = true;
  return true;
}

void CReceiver::Stop()
{
  m_source->Stop();

  m_stop = true;
  m_sampleCondition.notify_all();
  m_frameCondition.notify_all();
  if (m_demodulatorThread.joinable())
    m_demodulatorThread.join();
  if (m_decoderThread.joinable())
    m_decoderThread.join();

  m_demodulator.reset();
  m_radio.reset();
  m_agc.reset();
  m_samples.clear();
  m_frames.clear();

  m_source->Close();
  m_isStarted = false;
}

bool CReceiver::Tune(uint32_t frequency)
{
  if (!m_isStarted)
    return false;

  std::scoped_lock lock(m_demodulatorMutex, m_decoderMutex);

  if (!m_source->SetFrequency(frequency))
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to tune to {} Hz", frequency);
    return false;
  }

  // Destroying the demodulator joins its threads, so no stale frame arrives after the queue is
  // cleared below
  m_demodulator.reset();
  m_activeChannel = nullptr;
  m_attachedChannels.clear();
  m_labelDecoder = {};
  m_radio.reset();
  {
    std::lock_guard<std::mutex> sampleLock(m_sampleMutex);
    m_samples.clear();
    m_discardUntil = std::chrono::steady_clock::now() + DISCARD_AFTER_TUNE;
  }
  {
    std::lock_guard<std::mutex> frameLock(m_frameMutex);
    m_frames.clear();
  }

  const uint64_t generation = ++m_tuneGeneration;
  m_demodulator = Create_OFDM_Demodulator(TRANSMISSION_MODE, DEMODULATOR_THREADS);
  // The demodulator calls back while it exists, unlike m_demodulator while being replaced
  const OFDM_Demod* demodulator = m_demodulator.get();
  m_demodulator->On_OFDM_Frame().Attach(
      [this, generation, demodulator](tcb::span<const viterbi_bit_t> bits)
      {
        const float mer = CalculateFrameMer(*demodulator);
        m_mer = m_mer == 0.0f ? mer : Smooth(m_mer, mer);

        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        if (m_frames.size() >= MAX_QUEUED_FRAMES)
          m_frames.pop_front();
        m_frames.push_back({generation, {bits.begin(), bits.end()}});
        m_frameCondition.notify_one();
      });
  m_radio = std::make_unique<BasicRadio>(get_dab_parameters(TRANSMISSION_MODE), DECODER_THREADS);
  m_radio->On_Data_Packet_Channel().Attach(
      [this](subchannel_id_t subchannel, Basic_Data_Packet_Channel& channel)
      { AttachDataChannel(subchannel, channel); });
  m_frequency = frequency;

  if (m_agc)
  {
    m_agc->Reset();
    m_gain = m_agc->GetGain();
    m_source->SetGain(m_gain);
  }
  m_isSynced = false;
  m_framesRead = 0;
  m_framesDesynced = 0;
  m_mer = 0.0f;
  m_level = -100.0f;
  m_uncorrectable = 0;
  m_nextRetune = std::chrono::steady_clock::now() + RETUNE_WITHOUT_SIGNAL;

  Log(LogLevel::LEVEL_DEBUG, "Tuned to {} Hz", frequency);
  return true;
}

TunerStatus CReceiver::GetStatus() const
{
  return {.isSynced = m_isSynced,
          .framesRead = m_framesRead,
          .framesDesynced = m_framesDesynced,
          .gain = m_gain,
          .mer = m_mer,
          .level = m_level,
          .uncorrectable = m_uncorrectable};
}

std::optional<EnsembleInfo> CReceiver::GetEnsemble() const
{
  std::lock_guard<std::mutex> lock(m_decoderMutex);
  if (!m_radio)
    return {};

  std::lock_guard<std::mutex> radioLock(m_radio->GetMutex());
  return ReadEnsemble(m_radio->GetDatabase(), m_frequency);
}

bool CReceiver::HasSpiService() const
{
  std::lock_guard<std::mutex> lock(m_decoderMutex);
  if (!m_radio)
    return false;

  std::lock_guard<std::mutex> radioLock(m_radio->GetMutex());
  return std::ranges::any_of(m_radio->GetDatabase().service_components, IsSpiComponent);
}

void CReceiver::SelectService(uint16_t sid, uint8_t scids, IServiceListener* listener)
{
  std::lock_guard<std::mutex> lock(m_decoderMutex);
  if (m_activeChannel)
    m_activeChannel->GetControls().StopAll();
  m_activeChannel = nullptr;
  m_labelDecoder = {};
  m_uncorrectable = 0;
  m_selection = ServiceSelection{sid, scids, listener};
  UpdateServiceSelection();
}

void CReceiver::ClearService()
{
  std::lock_guard<std::mutex> lock(m_decoderMutex);
  if (m_activeChannel)
    m_activeChannel->GetControls().StopAll();
  m_activeChannel = nullptr;
  m_selection.reset();
}

void CReceiver::UpdateServiceSelection()
{
  if (!m_selection || !m_radio)
    return;

  std::optional<subchannel_id_t> subchannel;
  {
    std::lock_guard<std::mutex> radioLock(m_radio->GetMutex());
    for (const auto& component : m_radio->GetDatabase().service_components)
    {
      if (component.service_id.type == ServiceIdType::BITS16 &&
          component.service_id.get_unique_identifier() == m_selection->sid &&
          component.component_id == m_selection->scids &&
          component.transport_mode == TransportMode::STREAM_MODE_AUDIO)
      {
        subchannel = component.subchannel_id;
        break;
      }
    }
  }
  if (!subchannel)
    return;

  // Changes when the ensemble is reconfigured
  Basic_Audio_Channel* channel = m_radio->Get_Audio_Channel(*subchannel);
  if (!channel || channel == m_activeChannel)
    return;

  if (m_activeChannel)
    m_activeChannel->GetControls().StopAll();

  if (m_attachedChannels.insert(channel).second)
    AttachChannel(channel);

  channel->GetControls().RunAll();
  m_activeChannel = channel;
  Log(LogLevel::LEVEL_DEBUG, "Decoding service {:04X} from subchannel {}", m_selection->sid,
      *subchannel);
}

void CReceiver::AttachChannel(Basic_Audio_Channel* channel)
{
  // Callbacks cannot be detached, so they check whether their channel is still the active one
  if (auto* dabPlusChannel = dynamic_cast<Basic_DAB_Plus_Channel*>(channel))
  {
    dabPlusChannel->OnRSError().Attach(
        [this, channel](int, int)
        {
          if (channel == m_activeChannel)
            ++m_uncorrectable;
        });
  }

  channel->OnAudioData().Attach(
      [this, channel](BasicAudioParams params, tcb::span<const uint8_t> data)
      {
        if (channel != m_activeChannel || !m_selection)
          return;

        const AudioFormat format{params.frequency, static_cast<uint8_t>(params.is_stereo ? 2 : 1)};
        m_selection->listener->OnAudio(
            format, {reinterpret_cast<const int16_t*>(data.data()), data.size() / sizeof(int16_t)});
      });
  channel->OnDynamicLabelData().Attach(
      [this, channel](uint8_t toggle, std::string_view label)
      {
        if (channel == m_activeChannel && m_selection && m_labelDecoder.ProcessLabel(toggle, label))
          m_selection->listener->OnLabel(m_labelDecoder.GetLabel());
      });
  channel->OnDynamicLabelCommand().Attach(
      [this, channel](uint8_t, tcb::span<const uint8_t> data)
      {
        if (channel == m_activeChannel && m_selection &&
            m_labelDecoder.ProcessCommand({data.data(), data.size()}))
          m_selection->listener->OnLabel(m_labelDecoder.GetLabel());
      });
  channel->GetSlideshowManager().OnNewSlideshow().Attach(
      [this, channel](std::shared_ptr<Basic_Slideshow> slideshow)
      {
        if (channel != m_activeChannel || !m_selection ||
            slideshow->image_type == Basic_Image_Type::NONE)
          return;

        m_selection->listener->OnPicture(
            slideshow->image_type == Basic_Image_Type::PNG ? "image/png" : "image/jpeg",
            slideshow->image_data);
      });
}

void CReceiver::AttachDataChannel(uint8_t subchannel, Basic_Data_Packet_Channel& channel)
{
  // This runs while the radio holds its lock, so whether the channel carries SPI is checked when
  // an object arrives
  channel.OnMOTEntity().Attach(
      [this, subchannel](MOT_Entity entity)
      {
        IDataListener* listener = m_dataListener;
        if (!listener || !IsSpiSubchannel(subchannel))
          return;

        MotObject object;
        object.contentName = entity.header.content_name.value_or("");
        object.contentType = entity.header.content_type;
        object.contentSubType = entity.header.content_sub_type;
        for (const auto& parameter : entity.header.user_app_params)
          object.parameters[parameter.type] = parameter.data;
        object.body.assign(entity.body_buf.begin(), entity.body_buf.end());
        listener->OnMotObject(object);
      });

  // DAB-Radio passes images on as slideshows only
  channel.GetSlideshowManager().OnNewSlideshow().Attach(
      [this, subchannel](std::shared_ptr<Basic_Slideshow> slideshow)
      {
        IDataListener* listener = m_dataListener;
        if (!listener || !IsSpiSubchannel(subchannel))
          return;

        MotObject object;
        object.contentName = slideshow->name;
        object.contentType = MOT_CONTENT_TYPE_IMAGE;
        object.contentSubType = slideshow->image_type == Basic_Image_Type::PNG
                                    ? MOT_CONTENT_SUBTYPE_PNG
                                    : MOT_CONTENT_SUBTYPE_JPEG;
        object.body = slideshow->image_data;
        listener->OnMotObject(object);
      });
}

bool CReceiver::IsSpiSubchannel(uint8_t subchannel) const
{
  std::lock_guard<std::mutex> radioLock(m_radio->GetMutex());
  return std::ranges::any_of(
      m_radio->GetDatabase().service_components, [subchannel](const ServiceComponent& component)
      { return component.subchannel_id == subchannel && IsSpiComponent(component); });
}

void CReceiver::OnSamples(std::span<const uint8_t> samples)
{
  std::lock_guard<std::mutex> lock(m_sampleMutex);
  if (std::chrono::steady_clock::now() < m_discardUntil)
    return;

  if (m_samples.size() >= MAX_QUEUED_SAMPLE_BLOCKS)
  {
    Log(LogLevel::LEVEL_WARNING, "Demodulator is too slow, dropping samples");
    m_samples.clear();
  }
  m_samples.push_back({m_tuneGeneration, {samples.begin(), samples.end()}});
  m_sampleCondition.notify_one();
}

void CReceiver::DemodulateSamples()
{
  SampleBlock block;
  while (!m_stop)
  {
    {
      std::unique_lock<std::mutex> lock(m_sampleMutex);
      m_sampleCondition.wait(lock, [this] { return m_stop || !m_samples.empty(); });
      if (m_stop)
        break;
      block = std::move(m_samples.front());
      m_samples.pop_front();
    }

    std::lock_guard<std::mutex> lock(m_demodulatorMutex);
    if (!m_demodulator || block.generation != m_tuneGeneration)
      continue;

    if (m_agc)
    {
      if (const auto gain = m_agc->Process(block.samples))
      {
        m_gain = *gain;
        m_source->SetGain(*gain);
      }
    }

    const std::vector<uint8_t>& samples = block.samples;
    m_iq.resize(samples.size() / 2);
    for (size_t i = 0; i < m_iq.size(); ++i)
    {
      m_iq[i] = {(static_cast<float>(samples[2 * i]) - 127.5f) / 128.0f,
                 (static_cast<float>(samples[2 * i + 1]) - 127.5f) / 128.0f};
    }
    m_demodulator->Process({m_iq.data(), m_iq.size()});

    // The tuner gain is subtracted to make the level independent of the AGC
    const float level = CalculatePowerDb(m_iq) - static_cast<float>(m_gain) / 10.0f;
    m_level = m_level == -100.0f ? level : Smooth(m_level, level);

    m_isSynced = m_demodulator->GetState() == OFDM_Demod::State::READING_SYMBOLS;
    m_framesRead = m_demodulator->GetTotalFramesRead();
    m_framesDesynced = m_demodulator->GetTotalFramesDesync();

    if (m_framesRead == 0 && std::chrono::steady_clock::now() >= m_nextRetune)
    {
      Log(LogLevel::LEVEL_DEBUG, "No signal on {} Hz yet, setting the frequency again",
          m_frequency);
      m_source->SetFrequency(m_frequency);
      m_nextRetune = std::chrono::steady_clock::now() + RETUNE_WITHOUT_SIGNAL;
    }
  }
}

void CReceiver::DecodeFrames()
{
  Frame frame;
  while (!m_stop)
  {
    {
      std::unique_lock<std::mutex> lock(m_frameMutex);
      m_frameCondition.wait(lock, [this] { return m_stop || !m_frames.empty(); });
      if (m_stop)
        break;
      frame = std::move(m_frames.front());
      m_frames.pop_front();
    }

    std::lock_guard<std::mutex> lock(m_decoderMutex);
    if (m_radio && frame.generation == m_tuneGeneration)
    {
      m_radio->Process({frame.bits.data(), frame.bits.size()});
      UpdateServiceSelection();
    }
  }
}

} // namespace DABPLUS
