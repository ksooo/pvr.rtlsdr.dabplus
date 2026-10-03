/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "BandIII.h"

#include <algorithm>
#include <array>

namespace DABPLUS
{

namespace
{

// clang-format off
constexpr std::array BAND_III_BLOCKS{
    DabBlock{"5A", 174928000},  DabBlock{"5B", 176640000},  DabBlock{"5C", 178352000},
    DabBlock{"5D", 180064000},  DabBlock{"6A", 181936000},  DabBlock{"6B", 183648000},
    DabBlock{"6C", 185360000},  DabBlock{"6D", 187072000},  DabBlock{"7A", 188928000},
    DabBlock{"7B", 190640000},  DabBlock{"7C", 192352000},  DabBlock{"7D", 194064000},
    DabBlock{"8A", 195936000},  DabBlock{"8B", 197648000},  DabBlock{"8C", 199360000},
    DabBlock{"8D", 201072000},  DabBlock{"9A", 202928000},  DabBlock{"9B", 204640000},
    DabBlock{"9C", 206352000},  DabBlock{"9D", 208064000},  DabBlock{"10A", 209936000},
    DabBlock{"10N", 210096000}, DabBlock{"10B", 211648000}, DabBlock{"10C", 213360000},
    DabBlock{"10D", 215072000}, DabBlock{"11A", 216928000}, DabBlock{"11N", 217088000},
    DabBlock{"11B", 218640000}, DabBlock{"11C", 220352000}, DabBlock{"11D", 222064000},
    DabBlock{"12A", 223936000}, DabBlock{"12N", 224096000}, DabBlock{"12B", 225648000},
    DabBlock{"12C", 227360000}, DabBlock{"12D", 229072000}, DabBlock{"13A", 230784000},
    DabBlock{"13B", 232496000}, DabBlock{"13C", 234208000}, DabBlock{"13D", 235776000},
    DabBlock{"13E", 237488000}, DabBlock{"13F", 239200000},
};
// clang-format on

} // unnamed namespace

std::span<const DabBlock> GetBandIIIBlocks()
{
  return BAND_III_BLOCKS;
}

std::optional<DabBlock> FindBlockByFrequency(uint32_t frequency)
{
  const auto it = std::ranges::find(BAND_III_BLOCKS, frequency, &DabBlock::frequency);
  if (it == BAND_III_BLOCKS.cend())
    return {};

  return *it;
}

} // namespace DABPLUS
