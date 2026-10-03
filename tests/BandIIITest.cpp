/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/BandIII.h"

#include <algorithm>

#include <gtest/gtest.h>

using namespace DABPLUS;

TEST(BandIII, BlocksAreSortedAndUnique)
{
  const auto blocks = GetBandIIIBlocks();
  ASSERT_EQ(blocks.size(), 41u);
  EXPECT_EQ(blocks.front().label, "5A");
  EXPECT_EQ(blocks.back().label, "13F");
  EXPECT_TRUE(std::ranges::is_sorted(blocks, std::ranges::less{}, &DabBlock::frequency));
  EXPECT_EQ(std::ranges::adjacent_find(blocks, {}, &DabBlock::frequency), blocks.end());
}

TEST(BandIII, FindBlockByFrequency)
{
  const auto block = FindBlockByFrequency(178352000);
  ASSERT_TRUE(block);
  EXPECT_EQ(block->label, "5C");

  EXPECT_FALSE(FindBlockByFrequency(178352001));
}
