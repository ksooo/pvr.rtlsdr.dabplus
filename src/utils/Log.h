/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <string>
#include <utility>

#include <fmt/format.h>

namespace DABPLUS
{

enum class LogLevel
{
  LEVEL_DEBUG,
  LEVEL_INFO,
  LEVEL_WARNING,
  LEVEL_ERROR
};

/*!
 * \brief Writes a message to the Kodi log. The unit tests link a stand-in implementation.
 */
void WriteLog(LogLevel level, const std::string& message);

template<typename... Args>
void Log(LogLevel level, fmt::format_string<Args...> format, Args&&... args)
{
  WriteLog(level, fmt::format(format, std::forward<Args>(args)...));
}

} // namespace DABPLUS
