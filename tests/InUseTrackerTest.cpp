/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "device/InUseTracker.h"

#include <gtest/gtest.h>

using namespace DABPLUS;

TEST(InUseTracker, TimeoutWithoutServerInUseIsNotFound)
{
  CInUseTracker tracker;
  EXPECT_EQ(tracker.Resolve(OpenResult::TIMED_OUT), OpenResult::NOT_FOUND);
}

TEST(InUseTracker, TimeoutAfterServerInUseIsInUse)
{
  CInUseTracker tracker;
  EXPECT_EQ(tracker.Resolve(OpenResult::IN_USE), OpenResult::IN_USE);
  EXPECT_EQ(tracker.Resolve(OpenResult::TIMED_OUT), OpenResult::IN_USE);
  EXPECT_EQ(tracker.Resolve(OpenResult::TIMED_OUT), OpenResult::IN_USE);
}

TEST(InUseTracker, AnswerEndsInUse)
{
  CInUseTracker tracker;
  tracker.Resolve(OpenResult::IN_USE);
  EXPECT_EQ(tracker.Resolve(OpenResult::OPENED), OpenResult::OPENED);
  EXPECT_EQ(tracker.Resolve(OpenResult::TIMED_OUT), OpenResult::NOT_FOUND);

  tracker.Resolve(OpenResult::IN_USE);
  EXPECT_EQ(tracker.Resolve(OpenResult::NOT_FOUND), OpenResult::NOT_FOUND);
  EXPECT_EQ(tracker.Resolve(OpenResult::TIMED_OUT), OpenResult::NOT_FOUND);
}
