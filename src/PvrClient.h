/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "InstanceSettings.h"
#include "store/ChannelStore.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include <kodi/addon-instance/PVR.h>

namespace DABPLUS
{

class ATTR_DLL_LOCAL CPvrClient : public kodi::addon::CInstancePVRClient
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

private:
  InstanceSettings GetSettings() const;

  void RunChannelScan();
  void MonitorTuner();
  void UpdateConnectionState();
  void RequestTunerCheck();
  std::string GetChannelListPath() const;
  void LoadChannels();
  void SaveChannels() const;

  const unsigned int m_instanceNumber{0};

  // Read once from Kodi, which does not support reading settings from other threads
  mutable std::mutex m_settingsMutex;
  InstanceSettings m_settings;

  mutable std::mutex m_mutex;
  CChannelStore m_store;

  std::atomic<bool> m_isScanning{false};
  std::atomic<bool> m_abortScan{false};
  std::thread m_scanThread;

  std::mutex m_monitorMutex;
  std::condition_variable m_monitorCondition;
  bool m_stopMonitor{false};
  bool m_checkTuner{false};
  PVR_CONNECTION_STATE m_connectionState{PVR_CONNECTION_STATE_UNKNOWN};
  std::thread m_monitorThread;
};

} // namespace DABPLUS
