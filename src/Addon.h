/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/AddonBase.h>

namespace DABPLUS
{

class ATTR_DLL_LOCAL CAddon : public kodi::addon::CAddonBase
{
public:
  CAddon() = default;

  ADDON_STATUS CreateInstance(const kodi::addon::IInstanceInfo& instance,
                              KODI_ADDON_INSTANCE_HDL& hdl) override;
};

} // namespace DABPLUS
