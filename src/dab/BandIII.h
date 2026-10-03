/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace DABPLUS
{

struct DabBlock
{
  std::string_view label;
  uint32_t frequency{0}; // Hz
};

/*!
 * \brief The DAB frequency blocks of VHF Band III, in ascending order of frequency.
 */
std::span<const DabBlock> GetBandIIIBlocks();

std::optional<DabBlock> FindBlockByFrequency(uint32_t frequency);

} // namespace DABPLUS
