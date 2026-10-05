/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "device/SampleSource.h"

#include <atomic>

namespace DABPLUS
{

/*!
 * \brief Tells what a timed out connection attempt means.
 *
 * rtl_tcp keeps one connection waiting while it serves another client, and lets further attempts
 * time out. Once a waiting connection showed the server in use - and still occupies the queue -
 * a timeout means the same.
 */
class CInUseTracker
{
public:
  OpenResult Resolve(OpenResult result)
  {
    if (result == OpenResult::TIMED_OUT)
      return m_isInUse ? OpenResult::IN_USE : OpenResult::NOT_FOUND;

    m_isInUse = result == OpenResult::IN_USE;
    return result;
  }

private:
  std::atomic<bool> m_isInUse{false};
};

} // namespace DABPLUS
