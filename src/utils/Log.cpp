/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Log.h"

#include <kodi/AddonBase.h>

namespace DABPLUS
{

void WriteLog(LogLevel level, const std::string& message)
{
  ADDON_LOG kodiLevel{ADDON_LOG_DEBUG};
  switch (level)
  {
    case LogLevel::LEVEL_DEBUG:
      kodiLevel = ADDON_LOG_DEBUG;
      break;
    case LogLevel::LEVEL_INFO:
      kodiLevel = ADDON_LOG_INFO;
      break;
    case LogLevel::LEVEL_WARNING:
      kodiLevel = ADDON_LOG_WARNING;
      break;
    case LogLevel::LEVEL_ERROR:
      kodiLevel = ADDON_LOG_ERROR;
      break;
  }
  kodi::Log(kodiLevel, "%s", message.c_str());
}

} // namespace DABPLUS
