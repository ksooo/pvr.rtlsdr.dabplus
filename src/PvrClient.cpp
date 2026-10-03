/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "PvrClient.h"

#include "InstanceSettings.h"
#include "dab/BandIII.h"
#include "dab/Receiver.h"
#include "device/RtlTcpSource.h"
#include "scan/Scanner.h"
#include "stream/LiveStream.h"
#include "utils/Log.h"

#ifdef DABPLUS_HAS_USB
#include "device/RtlSdrUsbSource.h"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <utility>

#include <dab/constants/programme_type_table.h>
#include <kodi/Filesystem.h>
#include <kodi/gui/dialogs/OK.h>
#include <kodi/gui/dialogs/Progress.h>

namespace DABPLUS
{

namespace
{

constexpr uint32_t LABEL_SCAN_HEADING = 30100;
constexpr uint32_t LABEL_SCAN_BLOCK = 30101;
constexpr uint32_t LABEL_SCAN_FOUND = 30102;
constexpr uint32_t LABEL_NO_ENSEMBLES = 30103;
constexpr uint32_t LABEL_TUNER_ERROR = 30104;
constexpr uint32_t LABEL_NO_USB_DEVICE = 30105;
constexpr uint32_t LABEL_TCP_UNREACHABLE = 30106;
constexpr uint32_t LABEL_TUNER_IN_USE = 30107;
constexpr uint32_t LABEL_SIGNAL_SYNCED = 30108;
constexpr uint32_t LABEL_SIGNAL_NONE = 30109;

constexpr std::chrono::seconds TUNER_CHECK_INTERVAL{10};
constexpr std::chrono::seconds RECEIVER_LINGER{10};

// Covers synchronisation, receiving the ensemble information and the first audio frames
constexpr std::chrono::seconds AUDIO_TIMEOUT{8};
constexpr std::chrono::milliseconds DEMUX_READ_TIMEOUT{100};

constexpr int AUDIO_STREAM_ID = 1;
constexpr int METADATA_STREAM_ID = 2;

template<typename... Args>
std::string FormatLocalized(uint32_t labelId, Args&&... args)
{
  const std::string format = kodi::addon::GetLocalizedString(labelId);
  try
  {
    return fmt::format(fmt::runtime(format), std::forward<Args>(args)...);
  }
  catch (const fmt::format_error&)
  {
    return format;
  }
}

std::string DescribeTuner(const InstanceSettings& settings)
{
  if (settings.sourceType == SourceType::USB)
    return settings.usbSerial.empty() ? "USB" : fmt::format("USB {}", settings.usbSerial);

  return fmt::format("{}:{}", settings.tcpHost, settings.tcpPort);
}

std::unique_ptr<ISampleSource> CreateSampleSource(const InstanceSettings& settings)
{
#ifdef DABPLUS_HAS_USB
  if (settings.sourceType == SourceType::USB)
    return std::make_unique<CRtlSdrUsbSource>(settings.usbSerial, settings.ppmCorrection);
#endif

  return std::make_unique<CRtlTcpSource>(settings.tcpHost, settings.tcpPort,
                                         settings.ppmCorrection);
}

std::string GetGenre(uint8_t programmeType)
{
  if (programmeType == 0)
    return {};

  // International table 1 applies everywhere but North America, where DAB is not used
  return GetProgrammeTypeName(1, programmeType).long_label;
}

} // unnamed namespace

CPvrClient::CPvrClient(const kodi::addon::IInstanceInfo& instance)
  : kodi::addon::CInstancePVRClient(instance),
    m_instanceNumber(instance.GetNumber()),
    m_settings(ReadInstanceSettings(*this))
{
  LoadChannels();
  m_monitorThread = std::thread(&CPvrClient::MonitorTuner, this);
}

CPvrClient::~CPvrClient()
{
  {
    std::lock_guard<std::mutex> lock(m_monitorMutex);
    m_stopMonitor = true;
  }
  m_monitorCondition.notify_one();
  m_monitorThread.join();

  m_abortScan = true;
  if (m_scanThread.joinable())
    m_scanThread.join();

  std::lock_guard<std::mutex> lock(m_tunerMutex);
  StopReceiver();
}

ADDON_STATUS CPvrClient::SetInstanceSetting(const std::string& settingName,
                                            const kodi::addon::CSettingValue& settingValue)
{
  {
    std::lock_guard<std::mutex> lock(m_settingsMutex);
    UpdateInstanceSetting(m_settings, settingName, settingValue);
  }
  RequestTunerCheck();
  return ADDON_STATUS_OK;
}

PVR_ERROR CPvrClient::GetCapabilities(kodi::addon::PVRCapabilities& capabilities)
{
  capabilities.SetSupportsTV(false);
  capabilities.SetSupportsRadio(true);
  capabilities.SetSupportsChannelGroups(true);
  capabilities.SetSupportsChannelScan(true);
  capabilities.SetHandlesInputStream(true);
  capabilities.SetHandlesDemuxing(true);
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetBackendName(std::string& name)
{
  name = "RTL-SDR DAB+";
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetBackendVersion(std::string& version)
{
  version = kodi::addon::GetAddonInfo("version");
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetConnectionString(std::string& connection)
{
  connection = DescribeTuner(GetSettings());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetChannelsAmount(int& amount)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  amount = static_cast<int>(m_store.GetServices().size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results)
{
  if (!radio)
    return PVR_ERROR_NO_ERROR;

  std::lock_guard<std::mutex> lock(m_mutex);
  for (const auto& service : m_store.GetServices())
  {
    kodi::addon::PVRChannel channel;
    channel.SetUniqueId(static_cast<unsigned int>(service.GetUid()));
    channel.SetIsRadio(true);
    channel.SetChannelName(service.label);
    results.Add(channel);
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetChannelGroupsAmount(int& amount)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  amount = static_cast<int>(m_store.GetEnsembles().size());
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results)
{
  if (!radio)
    return PVR_ERROR_NO_ERROR;

  std::lock_guard<std::mutex> lock(m_mutex);
  unsigned int position{0};
  for (const auto& ensemble : m_store.GetEnsembles())
  {
    kodi::addon::PVRChannelGroup group;
    group.SetGroupName(m_store.GetGroupName(ensemble));
    group.SetIsRadio(true);
    group.SetPosition(++position);
    results.Add(group);
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                             kodi::addon::PVRChannelGroupMembersResultSet& results)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  for (const auto& ensemble : m_store.GetEnsembles())
  {
    const std::string groupName = m_store.GetGroupName(ensemble);
    if (groupName != group.GetGroupName())
      continue;

    for (const auto& service : ensemble.services)
    {
      kodi::addon::PVRChannelGroupMember member;
      member.SetGroupName(groupName);
      member.SetChannelUniqueId(static_cast<unsigned int>(service.GetUid()));
      results.Add(member);
    }
    break;
  }
  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR CPvrClient::OpenDialogChannelScan()
{
  // Kodi calls this on its GUI thread, which must keep running to render the progress dialog
  if (m_isScanning.exchange(true))
    return PVR_ERROR_NO_ERROR;

  bool isPlaying{false};
  {
    std::lock_guard<std::mutex> lock(m_tunerMutex);
    isPlaying = GetStream() != nullptr;
    if (!isPlaying)
      StopReceiver();
  }

  if (isPlaying)
  {
    m_isScanning = false;
    kodi::gui::dialogs::OK::ShowAndGetInput(kodi::addon::GetLocalizedString(LABEL_SCAN_HEADING),
                                            kodi::addon::GetLocalizedString(LABEL_TUNER_IN_USE));
    return PVR_ERROR_NO_ERROR;
  }

  if (m_scanThread.joinable())
    m_scanThread.join();

  m_scanThread = std::thread(
      [this]
      {
        RunChannelScan();
        m_isScanning = false;
        RequestTunerCheck();
      });
  return PVR_ERROR_NO_ERROR;
}

void CPvrClient::RunChannelScan()
{
  const InstanceSettings settings = GetSettings();
  CReceiver receiver(CreateSampleSource(settings), settings.gain);

  std::optional<std::vector<EnsembleInfo>> ensembles;
  {
    kodi::gui::dialogs::CProgress dialog;
    dialog.SetHeading(kodi::addon::GetLocalizedString(LABEL_SCAN_HEADING));
    dialog.SetCanCancel(true);
    dialog.ShowProgressBar(true);
    dialog.SetPercentage(0);
    dialog.Open();

    if (!receiver.Start())
    {
      kodi::gui::dialogs::OK::ShowAndGetInput(kodi::addon::GetLocalizedString(LABEL_SCAN_HEADING),
                                              kodi::addon::GetLocalizedString(LABEL_TUNER_ERROR));
      return;
    }

    Log(LogLevel::LEVEL_INFO, "Starting channel scan with {}", receiver.GetSourceName());

    const CScanner scanner;
    ensembles = scanner.Run(
        receiver, GetBandIIIBlocks(),
        [this, &dialog](const ScanProgress& progress)
        {
          dialog.SetLine(0, FormatLocalized(LABEL_SCAN_BLOCK, progress.block.label,
                                            progress.block.frequency / 1e6));
          dialog.SetLine(1, FormatLocalized(LABEL_SCAN_FOUND, progress.ensemblesFound,
                                            progress.servicesFound));
          dialog.SetPercentage(static_cast<int>(progress.blockIndex * 100 / progress.blockCount));
          return !dialog.IsCanceled() && !m_abortScan;
        });
    receiver.Stop();
  }

  if (!ensembles)
  {
    Log(LogLevel::LEVEL_INFO, "Channel scan cancelled");
    return;
  }

  if (ensembles->empty())
  {
    kodi::gui::dialogs::OK::ShowAndGetInput(kodi::addon::GetLocalizedString(LABEL_SCAN_HEADING),
                                            kodi::addon::GetLocalizedString(LABEL_NO_ENSEMBLES));
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_store.SetEnsembles(std::move(*ensembles));
    SaveChannels();
  }

  TriggerChannelUpdate();
  TriggerChannelGroupsUpdate();
}

PVR_ERROR CPvrClient::GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus)
{
  // Opening a stream holds the lock while waiting for the signal; report nothing meanwhile
  std::unique_lock<std::mutex> lock(m_tunerMutex, std::try_to_lock);
  if (!lock.owns_lock() || !m_receiver || !m_playingService)
    return PVR_ERROR_NO_ERROR;

  const TunerStatus status = m_receiver->GetStatus();
  signalStatus.SetAdapterName(m_receiver->GetSourceName());
  signalStatus.SetAdapterStatus(
      kodi::addon::GetLocalizedString(status.isSynced ? LABEL_SIGNAL_SYNCED : LABEL_SIGNAL_NONE));
  signalStatus.SetServiceName(m_playingService->label);
  signalStatus.SetMuxName(m_playingEnsemble);
  return PVR_ERROR_NO_ERROR;
}

bool CPvrClient::OpenLiveStream(const kodi::addon::PVRChannel& channel)
{
  std::lock_guard<std::mutex> lock(m_tunerMutex);
  if (m_isScanning)
  {
    Log(LogLevel::LEVEL_WARNING, "Unable to play while a channel scan is running");
    return false;
  }

  const int uid = static_cast<int>(channel.GetUniqueId());
  std::optional<ServiceInfo> service;
  std::vector<uint32_t> frequencies;
  {
    std::lock_guard<std::mutex> storeLock(m_mutex);
    service = m_store.FindService(uid);
    frequencies = m_store.GetFrequencies(uid);
  }
  if (!service || frequencies.empty() || !StartReceiver())
    return false;

  auto stream = std::make_shared<CLiveStream>(GetGenre(service->programmeType));
  for (const uint32_t frequency : frequencies)
  {
    if (frequency != m_receiverFrequency)
    {
      m_receiverFrequency = 0;
      if (!m_receiver->Tune(frequency))
        continue;
      m_receiverFrequency = frequency;
    }

    m_receiver->SelectService(service->sid, service->scids, stream.get());
    const auto format = stream->WaitForAudio(AUDIO_TIMEOUT);
    if (!format)
    {
      Log(LogLevel::LEVEL_WARNING, "No audio of service '{}' on {} Hz", service->label, frequency);
      m_receiver->ClearService();
      continue;
    }

    Log(LogLevel::LEVEL_INFO, "Playing service '{}' on {} Hz, {} Hz, {} channels", service->label,
        frequency, format->sampleRate, format->channels);
    {
      std::lock_guard<std::mutex> storeLock(m_mutex);
      if (frequencies.front() != frequency)
      {
        m_store.SetLastFrequency(uid, frequency);
        SaveChannels();
      }
      const auto ensemble = m_store.FindEnsemble(frequency);
      m_playingEnsemble = ensemble ? ensemble->label : std::string{};
    }
    m_playingService = service;
    {
      std::lock_guard<std::mutex> streamLock(m_streamMutex);
      m_stream = std::move(stream);
    }
    return true;
  }

  StopReceiver();
  return false;
}

void CPvrClient::CloseLiveStream()
{
  std::lock_guard<std::mutex> lock(m_tunerMutex);
  std::shared_ptr<CLiveStream> stream;
  {
    std::lock_guard<std::mutex> streamLock(m_streamMutex);
    stream = std::move(m_stream);
  }
  if (stream)
    stream->Abort();

  m_playingService.reset();
  if (!m_receiver)
    return;

  // The listener must not be used after the stream is gone
  m_receiver->ClearService();
  m_releaseReceiverAt = std::chrono::steady_clock::now() + RECEIVER_LINGER;
  RequestTunerCheck();
}

PVR_ERROR CPvrClient::GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& properties)
{
  const auto stream = GetStream();
  const auto format = stream ? stream->GetAudioFormat() : std::nullopt;
  if (!format)
    return PVR_ERROR_FAILED;

  const kodi::addon::PVRCodec audioCodec = GetCodecByName("pcm_s16le");
  kodi::addon::PVRStreamProperties audio;
  audio.SetPID(AUDIO_STREAM_ID);
  audio.SetCodecType(audioCodec.GetCodecType());
  audio.SetCodecId(audioCodec.GetCodecId());
  audio.SetChannels(format->channels);
  audio.SetSampleRate(static_cast<int>(format->sampleRate));
  audio.SetBitsPerSample(16);
  audio.SetBlockAlign(format->channels * 2);
  audio.SetBitRate(static_cast<int>(format->sampleRate) * format->channels * 16);
  properties.emplace_back(audio);

  const kodi::addon::PVRCodec metadataCodec = GetCodecByName("ID3");
  kodi::addon::PVRStreamProperties metadata;
  metadata.SetPID(METADATA_STREAM_ID);
  metadata.SetCodecType(metadataCodec.GetCodecType());
  metadata.SetCodecId(metadataCodec.GetCodecId());
  properties.emplace_back(metadata);

  return PVR_ERROR_NO_ERROR;
}

DEMUX_PACKET* CPvrClient::DemuxRead()
{
  const auto stream = GetStream();
  const auto packet = stream ? stream->Read(DEMUX_READ_TIMEOUT) : std::nullopt;
  if (!packet)
    return AllocateDemuxPacket(0);

  if (packet->type == StreamPacket::Type::FORMAT_CHANGE)
  {
    DEMUX_PACKET* demuxPacket = AllocateDemuxPacket(0);
    demuxPacket->iStreamId = DEMUX_SPECIALID_STREAMCHANGE;
    return demuxPacket;
  }

  DEMUX_PACKET* demuxPacket = AllocateDemuxPacket(static_cast<int>(packet->data.size()));
  if (!demuxPacket)
    return nullptr;

  std::memcpy(demuxPacket->pData, packet->data.data(), packet->data.size());
  demuxPacket->iSize = static_cast<int>(packet->data.size());
  demuxPacket->iStreamId =
      packet->type == StreamPacket::Type::AUDIO ? AUDIO_STREAM_ID : METADATA_STREAM_ID;
  demuxPacket->pts = static_cast<double>(packet->pts);
  demuxPacket->dts = demuxPacket->pts;
  demuxPacket->duration = static_cast<double>(packet->duration);
  return demuxPacket;
}

void CPvrClient::DemuxAbort()
{
  if (const auto stream = GetStream())
    stream->Abort();
}

void CPvrClient::DemuxFlush()
{
  if (const auto stream = GetStream())
    stream->Flush();
}

void CPvrClient::DemuxReset()
{
  DemuxFlush();
}

InstanceSettings CPvrClient::GetSettings() const
{
  std::lock_guard<std::mutex> lock(m_settingsMutex);
  return m_settings;
}

bool CPvrClient::StartReceiver()
{
  m_releaseReceiverAt.reset();
  if (m_receiver)
    return true;

  const InstanceSettings settings = GetSettings();
  auto receiver = std::make_unique<CReceiver>(CreateSampleSource(settings), settings.gain);
  if (!receiver->Start())
    return false;

  m_receiver = std::move(receiver);
  m_receiverFrequency = 0;
  return true;
}

void CPvrClient::StopReceiver()
{
  {
    std::lock_guard<std::mutex> streamLock(m_streamMutex);
    if (m_stream)
      m_stream->Abort();
    m_stream.reset();
  }
  m_playingService.reset();
  m_receiver.reset();
  m_receiverFrequency = 0;
  m_releaseReceiverAt.reset();
}

CPvrClient::TunerUsage CPvrClient::ReleaseIdleReceiver()
{
  std::lock_guard<std::mutex> lock(m_tunerMutex);
  if (m_releaseReceiverAt && std::chrono::steady_clock::now() >= *m_releaseReceiverAt)
  {
    Log(LogLevel::LEVEL_DEBUG, "Releasing the idle tuner");
    StopReceiver();
  }
  return {m_receiver != nullptr, m_releaseReceiverAt};
}

std::shared_ptr<CLiveStream> CPvrClient::GetStream() const
{
  std::lock_guard<std::mutex> lock(m_streamMutex);
  return m_stream;
}

void CPvrClient::MonitorTuner()
{
  std::unique_lock<std::mutex> lock(m_monitorMutex);
  while (!m_stopMonitor)
  {
    m_checkTuner = false;

    lock.unlock();
    const TunerUsage usage = ReleaseIdleReceiver();
    // Checking must not interfere with a tuner in use
    if (!usage.isInUse && !m_isScanning)
      UpdateConnectionState();
    lock.lock();

    auto wakeUp = std::chrono::steady_clock::now() + TUNER_CHECK_INTERVAL;
    if (usage.releaseAt)
      wakeUp = std::min(wakeUp, *usage.releaseAt);
    m_monitorCondition.wait_until(lock, wakeUp, [this] { return m_stopMonitor || m_checkTuner; });
  }
}

void CPvrClient::UpdateConnectionState()
{
  const InstanceSettings settings = GetSettings();
  const PVR_CONNECTION_STATE state = CreateSampleSource(settings)->IsAvailable()
                                         ? PVR_CONNECTION_STATE_CONNECTED
                                         : PVR_CONNECTION_STATE_SERVER_UNREACHABLE;
  if (state == m_connectionState)
    return;

  m_connectionState = state;

  std::string message;
  if (state != PVR_CONNECTION_STATE_CONNECTED)
    message = kodi::addon::GetLocalizedString(
        settings.sourceType == SourceType::USB ? LABEL_NO_USB_DEVICE : LABEL_TCP_UNREACHABLE);

  ConnectionStateChange(DescribeTuner(settings), state, message);
}

void CPvrClient::RequestTunerCheck()
{
  {
    std::lock_guard<std::mutex> lock(m_monitorMutex);
    m_checkTuner = true;
  }
  m_monitorCondition.notify_one();
}

std::string CPvrClient::GetChannelListPath() const
{
  // GetUserPath(append) adds a separator even if the user path ends with one. Kodi's directory
  // cache does not match such paths, so the file could not be opened again.
  std::string path = kodi::addon::GetUserPath();
  if (!path.empty() && path.back() != '/' && path.back() != '\\')
    path += '/';
  return path + fmt::format("channels-{}.json", m_instanceNumber);
}

void CPvrClient::LoadChannels()
{
  const std::string path = GetChannelListPath();
  if (!kodi::vfs::FileExists(path))
    return;

  kodi::vfs::CFile file;
  if (!file.OpenFile(path))
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to open channel list '{}'", path);
    return;
  }

  std::string json;
  std::array<char, 4096> buffer{};
  ssize_t read{0};
  while ((read = file.Read(buffer.data(), buffer.size())) > 0)
    json.append(buffer.data(), static_cast<size_t>(read));

  std::lock_guard<std::mutex> lock(m_mutex);
  m_store.FromJson(json);
}

void CPvrClient::SaveChannels() const
{
  kodi::vfs::CreateDirectory(kodi::addon::GetUserPath());

  const std::string path = GetChannelListPath();
  kodi::vfs::CFile file;
  if (!file.OpenFileForWrite(path, true))
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to write channel list '{}'", path);
    return;
  }

  const std::string json = m_store.ToJson();
  file.Write(json.data(), json.size());
}

} // namespace DABPLUS
