/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Receiver.h"

#include "dab/EnsembleReader.h"
#include "device/SoftwareAgc.h"
#include "utils/Log.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <utility>

#include <basic_radio/basic_radio.h>
#include <dab/constants/dab_parameters.h>
#include <dab/database/dab_database.h>
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
  m_demodulator->On_OFDM_Frame().Attach(
      [this, generation](tcb::span<const viterbi_bit_t> bits)
      {
        std::lock_guard<std::mutex> frameLock(m_frameMutex);
        if (m_frames.size() >= MAX_QUEUED_FRAMES)
          m_frames.pop_front();
        m_frames.push_back({generation, {bits.begin(), bits.end()}});
        m_frameCondition.notify_one();
      });
  m_radio = std::make_unique<BasicRadio>(get_dab_parameters(TRANSMISSION_MODE), DECODER_THREADS);
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

  Log(LogLevel::LEVEL_DEBUG, "Tuned to {} Hz", frequency);
  return true;
}

TunerStatus CReceiver::GetStatus() const
{
  return {m_isSynced, m_framesRead, m_framesDesynced, m_gain};
}

std::optional<EnsembleInfo> CReceiver::GetEnsemble() const
{
  std::lock_guard<std::mutex> lock(m_decoderMutex);
  if (!m_radio)
    return {};

  std::lock_guard<std::mutex> radioLock(m_radio->GetMutex());
  return ReadEnsemble(m_radio->GetDatabase(), m_frequency);
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

    m_isSynced = m_demodulator->GetState() == OFDM_Demod::State::READING_SYMBOLS;
    m_framesRead = m_demodulator->GetTotalFramesRead();
    m_framesDesynced = m_demodulator->GetTotalFramesDesync();
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
      m_radio->Process({frame.bits.data(), frame.bits.size()});
  }
}

} // namespace DABPLUS
