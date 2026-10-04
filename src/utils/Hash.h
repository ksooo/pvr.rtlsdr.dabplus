/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <span>

namespace DABPLUS
{

/*!
 * \brief 64 bit FNV-1a hash; unlike std::hash stable across runs.
 */
constexpr uint64_t HashData(std::span<const uint8_t> data)
{
  uint64_t hash{0xcbf29ce484222325};
  for (const uint8_t byte : data)
  {
    hash ^= byte;
    hash *= 0x100000001b3;
  }
  return hash;
}

} // namespace DABPLUS
