/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "utils/Log.h"

#include <cstdlib>
#include <iostream>

namespace DABPLUS
{

void WriteLog(LogLevel level, const std::string& message)
{
  if (std::getenv("DABPLUS_TEST_LOG"))
    std::cerr << static_cast<int>(level) << ": " << message << std::endl;
}

} // namespace DABPLUS
