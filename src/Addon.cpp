/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Addon.h"

#include "PvrClient.h"

namespace DABPLUS
{

ADDON_STATUS CAddon::CreateInstance(const kodi::addon::IInstanceInfo& instance,
                                    KODI_ADDON_INSTANCE_HDL& hdl)
{
  if (!instance.IsType(ADDON_INSTANCE_PVR))
    return ADDON_STATUS_UNKNOWN;

  hdl = new CPvrClient(instance);
  return ADDON_STATUS_OK;
}

} // namespace DABPLUS

ADDONCREATOR(DABPLUS::CAddon)
