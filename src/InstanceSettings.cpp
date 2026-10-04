/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "InstanceSettings.h"

#include <kodi/AddonBase.h>

namespace DABPLUS
{

InstanceSettings ReadInstanceSettings(kodi::addon::IAddonInstance& instance)
{
  InstanceSettings settings;
  settings.sourceType = instance.GetInstanceSettingEnum<SourceType>("source_type");
  settings.usbSerial = instance.GetInstanceSettingString("usb_serial");
  settings.tcpHost = instance.GetInstanceSettingString("tcp_host");
  settings.tcpPort = static_cast<uint16_t>(instance.GetInstanceSettingInt("tcp_port", 1234));
  settings.gain.automatic = instance.GetInstanceSettingBoolean("gain_auto", true);
  settings.gain.manualGain = instance.GetInstanceSettingInt("gain_manual", 30) * 10;
  settings.ppmCorrection = instance.GetInstanceSettingInt("ppm_correction");
  settings.backgroundUpdate = instance.GetInstanceSettingBoolean("background_update", true);

#ifndef DABPLUS_HAS_USB
  settings.sourceType = SourceType::RTL_TCP;
#endif

  return settings;
}

void UpdateInstanceSetting(InstanceSettings& settings,
                           const std::string& name,
                           const kodi::addon::CSettingValue& value)
{
  if (name == "source_type")
    settings.sourceType = value.GetEnum<SourceType>();
  else if (name == "usb_serial")
    settings.usbSerial = value.GetString();
  else if (name == "tcp_host")
    settings.tcpHost = value.GetString();
  else if (name == "tcp_port")
    settings.tcpPort = static_cast<uint16_t>(value.GetInt());
  else if (name == "gain_auto")
    settings.gain.automatic = value.GetBoolean();
  else if (name == "gain_manual")
    settings.gain.manualGain = value.GetInt() * 10;
  else if (name == "ppm_correction")
    settings.ppmCorrection = value.GetInt();
  else if (name == "background_update")
    settings.backgroundUpdate = value.GetBoolean();

#ifndef DABPLUS_HAS_USB
  settings.sourceType = SourceType::RTL_TCP;
#endif
}

} // namespace DABPLUS
