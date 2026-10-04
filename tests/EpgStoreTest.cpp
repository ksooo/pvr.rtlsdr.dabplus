/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "store/EpgStore.h"

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

constexpr int UID = 0xED2100;
constexpr std::time_t DAY = 86400;
constexpr std::time_t SCOPE_START = 1791064800;

EpgEvent MakeEvent(uint32_t id, std::time_t start, std::time_t duration, std::string title)
{
  return {.id = id, .start = start, .end = start + duration, .title = std::move(title)};
}

} // unnamed namespace

TEST(EpgStore, ApplyScheduleReportsChanges)
{
  CEpgStore store;
  const EpgChanges initial = store.ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY,
                                                 {MakeEvent(1, SCOPE_START, 300, "Nachrichten"),
                                                  MakeEvent(2, SCOPE_START + 300, 3600, "A"),
                                                  MakeEvent(3, SCOPE_START + 3900, 3600, "B")});
  EXPECT_EQ(initial.created.size(), 3u);
  EXPECT_TRUE(initial.updated.empty());
  EXPECT_TRUE(initial.deleted.empty());

  const EpgChanges changes = store.ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY,
                                                 {MakeEvent(1, SCOPE_START, 300, "Nachrichten"),
                                                  MakeEvent(2, SCOPE_START + 300, 3600, "A2"),
                                                  MakeEvent(4, SCOPE_START + 3900, 3600, "C")});
  ASSERT_EQ(changes.created.size(), 1u);
  EXPECT_EQ(changes.created[0].id, 4u);
  ASSERT_EQ(changes.updated.size(), 1u);
  EXPECT_EQ(changes.updated[0].title, "A2");
  ASSERT_EQ(changes.deleted.size(), 1u);
  EXPECT_EQ(changes.deleted[0].id, 3u);

  EXPECT_TRUE(store
                  .ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY,
                                 {MakeEvent(1, SCOPE_START, 300, "Nachrichten"),
                                  MakeEvent(2, SCOPE_START + 300, 3600, "A2"),
                                  MakeEvent(4, SCOPE_START + 3900, 3600, "C")})
                  .IsEmpty());
}

TEST(EpgStore, EventsOutsideScopeAreKept)
{
  CEpgStore store;
  store.ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY,
                      {MakeEvent(1, SCOPE_START, 300, "Today")});
  const EpgChanges changes =
      store.ApplySchedule(UID, SCOPE_START + DAY, SCOPE_START + 2 * DAY,
                          {MakeEvent(2, SCOPE_START + DAY, 300, "Tomorrow")});

  EXPECT_TRUE(changes.deleted.empty());
  EXPECT_EQ(store.GetEvents(UID, SCOPE_START, SCOPE_START + 2 * DAY).size(), 2u);
}

TEST(EpgStore, ScopeOfEventsIfUnset)
{
  CEpgStore store;
  store.ApplySchedule(
      UID, 0, 0,
      {MakeEvent(1, SCOPE_START, 300, "A"), MakeEvent(2, SCOPE_START + DAY, 300, "Other day")});
  const EpgChanges changes =
      store.ApplySchedule(UID, 0, 0, {MakeEvent(3, SCOPE_START + 600, 300, "B")});

  EXPECT_TRUE(changes.deleted.empty());
  EXPECT_EQ(store.GetEvents(UID, 0, SCOPE_START + 2 * DAY).size(), 3u);

  EXPECT_TRUE(store.ApplySchedule(UID, 0, 0, {}).IsEmpty());
}

TEST(EpgStore, GetEventsOverlappingTimeSpan)
{
  CEpgStore store;
  store.ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY,
                      {MakeEvent(3, SCOPE_START + 7200, 3600, "C"),
                       MakeEvent(1, SCOPE_START, 3600, "A"),
                       MakeEvent(2, SCOPE_START + 3600, 3600, "B")});

  const auto events = store.GetEvents(UID, SCOPE_START + 1800, SCOPE_START + 7200);
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].title, "A");
  EXPECT_EQ(events[1].title, "B");
  EXPECT_TRUE(store.GetEvents(UID + 1, 0, SCOPE_START + DAY).empty());
}

TEST(EpgStore, RemoveEndedBefore)
{
  CEpgStore store;
  store.ApplySchedule(
      UID, SCOPE_START, SCOPE_START + DAY,
      {MakeEvent(1, SCOPE_START, 3600, "A"), MakeEvent(2, SCOPE_START + 3600, 3600, "B")});
  store.RemoveEndedBefore(SCOPE_START + 3601);

  const auto events = store.GetEvents(UID, 0, SCOPE_START + DAY);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].title, "B");
}

TEST(EpgStore, JsonRoundTrip)
{
  CEpgStore store;
  EpgEvent event = MakeEvent(1, SCOPE_START, 3600, "Lange Nacht");
  event.plotOutline = "Kurz";
  event.plot = "Lang";
  event.genre = "Hörspiel";
  store.ApplySchedule(UID, SCOPE_START, SCOPE_START + DAY, {event});
  store.SetLastUpdate(SCOPE_START);

  CEpgStore restored;
  ASSERT_TRUE(restored.FromJson(store.ToJson()));
  const auto events = restored.GetEvents(UID, 0, SCOPE_START + DAY);
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0], event);
  EXPECT_EQ(restored.GetLastUpdate(), SCOPE_START);

  EXPECT_FALSE(restored.FromJson("{"));
  EXPECT_FALSE(restored.FromJson(R"({"version": 99, "channels": {}})"));
  EXPECT_EQ(restored.GetEvents(UID, 0, SCOPE_START + DAY).size(), 1u);
}
