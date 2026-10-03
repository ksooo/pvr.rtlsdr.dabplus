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
#include "utils/Log.h"

#ifdef DABPLUS_HAS_USB
#include "device/RtlSdrUsbSource.h"
#endif

#include <array>
#include <chrono>
#include <memory>
#include <utility>

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

constexpr std::chrono::seconds TUNER_CHECK_INTERVAL{10};

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

InstanceSettings CPvrClient::GetSettings() const
{
  std::lock_guard<std::mutex> lock(m_settingsMutex);
  return m_settings;
}

void CPvrClient::MonitorTuner()
{
  std::unique_lock<std::mutex> lock(m_monitorMutex);
  while (!m_stopMonitor)
  {
    m_checkTuner = false;

    // Checking must not interfere with a tuner in use
    if (!m_isScanning)
    {
      lock.unlock();
      UpdateConnectionState();
      lock.lock();
    }

    m_monitorCondition.wait_for(lock, TUNER_CHECK_INTERVAL,
                                [this] { return m_stopMonitor || m_checkTuner; });
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
