/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "device/SoftwareAgc.h"

#include <optional>
#include <vector>

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

const std::vector<int> R820T_GAINS{0,   9,   14,  27,  37,  77,  87,  125, 144, 157,
                                   166, 197, 207, 229, 254, 280, 297, 328, 338, 364,
                                   372, 386, 402, 421, 434, 439, 445, 480, 496};

// Samples alternating between 128 - amplitude and 128 + amplitude
std::vector<uint8_t> MakeBlock(int amplitude)
{
  std::vector<uint8_t> block(65536);
  for (size_t i = 0; i < block.size(); ++i)
    block[i] = static_cast<uint8_t>(i % 2 ? 128 + amplitude : 128 - amplitude);
  return block;
}

std::optional<int> FeedBlocks(CSoftwareAgc& agc, int amplitude, int count)
{
  const auto block = MakeBlock(amplitude);
  std::optional<int> lastChange;
  for (int i = 0; i < count; ++i)
  {
    if (const auto gain = agc.Process(block))
      lastChange = gain;
  }
  return lastChange;
}

} // unnamed namespace

TEST(SoftwareAgc, StartsInTheMiddle)
{
  const CSoftwareAgc agc(R820T_GAINS);
  EXPECT_EQ(agc.GetGain(), R820T_GAINS[R820T_GAINS.size() / 2]);
}

TEST(SoftwareAgc, LowersGainWhenClipping)
{
  CSoftwareAgc agc(R820T_GAINS);
  const int initialGain = agc.GetGain();

  const auto gain = FeedBlocks(agc, 127, 4);
  ASSERT_TRUE(gain);
  EXPECT_LT(*gain, initialGain);
  EXPECT_EQ(*gain, agc.GetGain());
}

TEST(SoftwareAgc, RaisesGainOnWeakSignal)
{
  CSoftwareAgc agc(R820T_GAINS);
  const int initialGain = agc.GetGain();

  const auto gain = FeedBlocks(agc, 10, 4);
  ASSERT_TRUE(gain);
  EXPECT_GT(*gain, initialGain);
}

TEST(SoftwareAgc, KeepsGainOnGoodSignal)
{
  CSoftwareAgc agc(R820T_GAINS);
  EXPECT_FALSE(FeedBlocks(agc, 100, 40));
}

TEST(SoftwareAgc, DecidesOnlyEveryFewBlocks)
{
  CSoftwareAgc agc(R820T_GAINS);
  EXPECT_FALSE(FeedBlocks(agc, 127, 3));
  EXPECT_TRUE(FeedBlocks(agc, 127, 1));
}

TEST(SoftwareAgc, DoesNotReturnToOverloadingGain)
{
  CSoftwareAgc agc(R820T_GAINS);
  const int initialGain = agc.GetGain();

  ASSERT_TRUE(FeedBlocks(agc, 127, 4));
  FeedBlocks(agc, 1, 4 * static_cast<int>(R820T_GAINS.size()));
  EXPECT_LT(agc.GetGain(), initialGain);

  agc.Reset();
  FeedBlocks(agc, 1, 4 * static_cast<int>(R820T_GAINS.size()));
  EXPECT_EQ(agc.GetGain(), R820T_GAINS.back());
}

TEST(SoftwareAgc, StaysWithinGainRange)
{
  CSoftwareAgc agc(R820T_GAINS);
  FeedBlocks(agc, 1, 4 * static_cast<int>(R820T_GAINS.size()));
  EXPECT_EQ(agc.GetGain(), R820T_GAINS.back());

  FeedBlocks(agc, 127, 4 * static_cast<int>(R820T_GAINS.size()));
  EXPECT_EQ(agc.GetGain(), R820T_GAINS.front());
}

TEST(SoftwareAgc, ResetReturnsToTheMiddle)
{
  CSoftwareAgc agc(R820T_GAINS);
  FeedBlocks(agc, 127, 8);
  agc.Reset();
  EXPECT_EQ(agc.GetGain(), R820T_GAINS[R820T_GAINS.size() / 2]);
}

TEST(SoftwareAgc, WithoutGainsNothingChanges)
{
  CSoftwareAgc agc({});
  EXPECT_EQ(agc.GetGain(), 0);
  EXPECT_FALSE(FeedBlocks(agc, 127, 8));
}
