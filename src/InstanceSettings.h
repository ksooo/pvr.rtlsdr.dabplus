/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/Receiver.h"

#include <cstdint>
#include <string>

namespace kodi::addon
{
class CSettingValue;
class IAddonInstance;
} // namespace kodi::addon

namespace DABPLUS
{

enum class SourceType
{
  USB = 0,
  RTL_TCP = 1
};

struct InstanceSettings
{
  SourceType sourceType{SourceType::RTL_TCP};
  std::string usbSerial;
  std::string tcpHost;
  uint16_t tcpPort{1234};
  GainConfig gain;
  int ppmCorrection{0};
  bool backgroundUpdate{true};
};

InstanceSettings ReadInstanceSettings(kodi::addon::IAddonInstance& instance);

/*!
 * \brief Applies a changed setting as passed to kodi::addon::IAddonInstance::SetInstanceSetting().
 */
void UpdateInstanceSetting(InstanceSettings& settings,
                           const std::string& name,
                           const kodi::addon::CSettingValue& value);

} // namespace DABPLUS
