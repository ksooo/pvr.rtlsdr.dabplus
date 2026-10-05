/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "InstanceSettings.h"
#include "device/InUseTracker.h"
#include "spi/SpiCollector.h"
#include "store/ChannelStore.h"
#include "store/EpgStore.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <kodi/addon-instance/PVR.h>

namespace DABPLUS
{

class CLiveStream;
class CReceiver;

class ATTR_DLL_LOCAL CPvrClient : public kodi::addon::CInstancePVRClient, private ISpiHandler
{
public:
  explicit CPvrClient(const kodi::addon::IInstanceInfo& instance);
  ~CPvrClient() override;

  ADDON_STATUS SetInstanceSetting(const std::string& settingName,
                                  const kodi::addon::CSettingValue& settingValue) override;

  PVR_ERROR GetCapabilities(kodi::addon::PVRCapabilities& capabilities) override;
  PVR_ERROR GetBackendName(std::string& name) override;
  PVR_ERROR GetBackendVersion(std::string& version) override;
  PVR_ERROR GetConnectionString(std::string& connection) override;

  PVR_ERROR GetChannelsAmount(int& amount) override;
  PVR_ERROR GetChannels(bool radio, kodi::addon::PVRChannelsResultSet& results) override;
  PVR_ERROR GetChannelGroupsAmount(int& amount) override;
  PVR_ERROR GetChannelGroups(bool radio, kodi::addon::PVRChannelGroupsResultSet& results) override;
  PVR_ERROR GetChannelGroupMembers(const kodi::addon::PVRChannelGroup& group,
                                   kodi::addon::PVRChannelGroupMembersResultSet& results) override;
  PVR_ERROR OpenDialogChannelScan() override;
  PVR_ERROR CallSettingsMenuHook(const kodi::addon::PVRMenuhook& menuhook) override;
  PVR_ERROR GetSignalStatus(int channelUid, kodi::addon::PVRSignalStatus& signalStatus) override;

  PVR_ERROR GetEPGForChannel(int channelUid,
                             time_t start,
                             time_t end,
                             kodi::addon::PVREPGTagsResultSet& results) override;
  PVR_ERROR SetEPGMaxPastDays(int pastDays) override;
  PVR_ERROR SetEPGMaxFutureDays(int futureDays) override;

  PVR_ERROR OnSystemSleep() override;
  PVR_ERROR OnSystemWake() override;

  bool OpenLiveStream(const kodi::addon::PVRChannel& channel) override;
  void CloseLiveStream() override;
  bool IsRealTimeStream() override { return true; }
  PVR_ERROR GetStreamProperties(std::vector<kodi::addon::PVRStreamProperties>& properties) override;
  DEMUX_PACKET* DemuxRead() override;
  void DemuxAbort() override;
  void DemuxFlush() override;
  void DemuxReset() override;

private:
  struct TunerUsage
  {
    bool isInUse{false};
    std::optional<std::chrono::steady_clock::time_point> releaseAt;
  };

  InstanceSettings GetSettings() const;
  OpenResult StartReceiver();
  void StopReceiver();
  TunerUsage ReleaseIdleReceiver();
  std::shared_ptr<CLiveStream> GetStream() const;

  void OnLogo(const std::vector<SpiServiceId>& services,
              const std::vector<uint8_t>& image) override;
  void OnSchedule(const SpiSchedule& schedule) override;
  std::vector<int> FindChannels(const std::vector<SpiServiceId>& services) const;
  bool IsInEpgTimeFrame(const EpgEvent& event) const;
  void PushEpgEvents(int uid, const std::vector<EpgEvent>& events, EPG_EVENT_STATE state);
  void PushAllEpgEvents();
  void PushNewEpgEvents();
  void RefreshEpg();

  void StartBackgroundUpdateIfDue();
  void StartBackgroundUpdate(bool isManual);
  void StopBackgroundUpdate();
  void RunBackgroundUpdate(bool isManual);

  void RunChannelScan();
  void MonitorTuner();
  void UpdateConnectionState();
  void RequestTunerCheck();
  std::string GetUserFilePath(const std::string& name) const;
  std::string GetLogoPath(const std::string& fileName) const;
  void LoadChannels();
  void SaveChannels() const;
  void LoadEpg();
  void SaveEpg() const;

  const unsigned int m_instanceNumber{0};

  // Read once from Kodi, which does not support reading settings from other threads
  mutable std::mutex m_settingsMutex;
  InstanceSettings m_settings;

  mutable std::mutex m_mutex;
  CChannelStore m_store;

  mutable std::mutex m_epgMutex;
  CEpgStore m_epgStore;
  std::atomic<int> m_epgPastDays{EPG_TIMEFRAME_UNLIMITED};
  std::atomic<int> m_epgFutureDays{EPG_TIMEFRAME_UNLIMITED};
  //! Events starting before this time have been pushed to Kodi
  std::time_t m_epgPushedUntil{0};

  // Must outlive the receiver
  CSpiCollector m_spiCollector{*this};

  std::atomic<bool> m_isScanning{false};
  std::atomic<bool> m_abortScan{false};
  std::thread m_scanThread;

  std::mutex m_monitorMutex;
  std::condition_variable m_monitorCondition;
  bool m_stopMonitor{false};
  bool m_checkTuner{false};
  std::chrono::steady_clock::time_point m_nextEpgRefresh;
  std::chrono::steady_clock::time_point m_backgroundUpdateNotBefore;
  std::atomic<bool> m_isSleeping{false};
  PVR_CONNECTION_STATE m_connectionState{PVR_CONNECTION_STATE_UNKNOWN};
  CInUseTracker m_inUse;
  std::thread m_monitorThread;

  // The receiver keeps running for a while after playback stopped, to switch quickly to another
  // service of the same ensemble
  std::mutex m_tunerMutex;
  std::unique_ptr<CReceiver> m_receiver;
  uint32_t m_receiverFrequency{0};
  std::optional<std::chrono::steady_clock::time_point> m_releaseReceiverAt;
  std::optional<ServiceInfo> m_playingService;
  std::string m_playingEnsemble;

  mutable std::mutex m_streamMutex;
  std::shared_ptr<CLiveStream> m_stream;

  // Receives the SPI data of all ensembles while the tuner is not used otherwise
  std::mutex m_backgroundMutex;
  std::thread m_backgroundThread;
  std::atomic<bool> m_isBackgroundUpdateRunning{false};
  std::atomic<bool> m_isManualUpdate{false};
  std::atomic<bool> m_abortBackgroundUpdate{false};
  //! The frequencies not updated yet; only used by the background thread
  std::vector<uint32_t> m_backgroundPending;
};

} // namespace DABPLUS
