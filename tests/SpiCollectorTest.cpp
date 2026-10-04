/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiTestData.h"
#include "spi/SpiCollector.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace SPI_TEST;

namespace
{

constexpr std::time_t SCOPE_START = 1791064800;

class CRecordingHandler : public ISpiHandler
{
public:
  void OnLogo(const std::vector<SpiServiceId>& services, const std::vector<uint8_t>& image) override
  {
    logos.emplace_back(services, image);
  }

  void OnSchedule(const SpiSchedule& schedule) override { schedules.emplace_back(schedule); }

  std::vector<std::pair<std::vector<SpiServiceId>, std::vector<uint8_t>>> logos;
  std::vector<SpiSchedule> schedules;
};

MotObject MakeServiceInformation()
{
  return {.contentName = "si.EIB",
          .contentType = 7,
          .contentSubType = 0,
          .body = ServiceInformation(
              {Service(Bearer(0xE0, 0x10BC, 0xD210),
                       {Logo("d210_32x32.png", 32, 32), Logo("d210_128x128.png", 128, 128),
                        Logo("d210_112x32.png", 112, 32)}),
               Service(Bearer(0xE0, 0x10BC, 0xD220), {Logo("d220_320x240.png", 320, 240)})})};
}

MotObject MakeImage(std::string name, std::vector<uint8_t> data)
{
  return {.contentName = std::move(name), .contentType = 2, .contentSubType = 3, .body = data};
}

MotObject MakeProgrammeInformation(const Bytes& scheduleContent)
{
  return {.contentName = "w20261004dd210c0.EHB",
          .contentType = 7,
          .contentSubType = 1,
          .body = ProgrammeInformation(scheduleContent)};
}

} // unnamed namespace

TEST(SpiCollector, LargestLogoAfterServiceInformation)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  collector.OnMotObject(MakeServiceInformation());
  collector.OnMotObject(MakeImage("d210_32x32.png", {1}));
  collector.OnMotObject(MakeImage("d210_112x32.png", {2}));
  EXPECT_TRUE(handler.logos.empty());

  collector.OnMotObject(MakeImage("d210_128x128.png", {3}));
  ASSERT_EQ(handler.logos.size(), 1u);
  ASSERT_EQ(handler.logos[0].first.size(), 1u);
  EXPECT_EQ(handler.logos[0].first[0].sid, 0xD210u);
  EXPECT_EQ(handler.logos[0].second, (std::vector<uint8_t>{3}));
}

TEST(SpiCollector, LargestLogoRegardlessOfAspectRatio)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  MotObject serviceInformation = MakeServiceInformation();
  serviceInformation.body = ServiceInformation(
      {Service(Bearer(0xE0, 0x10BC, 0xD230),
               {Logo("d230_128x128.png", 128, 128), Logo("d230_320x240.png", 320, 240)})});
  collector.OnMotObject(serviceInformation);
  collector.OnMotObject(MakeImage("d230_128x128.png", {1}));
  EXPECT_TRUE(handler.logos.empty());

  collector.OnMotObject(MakeImage("d230_320x240.png", {2}));
  ASSERT_EQ(handler.logos.size(), 1u);
  EXPECT_EQ(handler.logos[0].first[0].sid, 0xD230u);
}

TEST(SpiCollector, ImagesBeforeServiceInformation)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  collector.OnMotObject(MakeImage("d210_128x128.png", {3}));
  collector.OnMotObject(MakeImage("unrelated.png", {5}));
  EXPECT_TRUE(handler.logos.empty());

  collector.OnMotObject(MakeServiceInformation());
  ASSERT_EQ(handler.logos.size(), 1u);
  EXPECT_EQ(handler.logos[0].second, (std::vector<uint8_t>{3}));
}

TEST(SpiCollector, RepeatedObjectsArePassedOnOnce)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  const MotObject programmeInformation =
      MakeProgrammeInformation(Programme(1, "Nachrichten", SCOPE_START, 300));
  for (int i = 0; i < 3; ++i)
  {
    collector.OnMotObject(MakeServiceInformation());
    collector.OnMotObject(MakeImage("d210_128x128.png", {3}));
    collector.OnMotObject(programmeInformation);
  }
  EXPECT_EQ(handler.logos.size(), 1u);
  EXPECT_EQ(handler.schedules.size(), 1u);

  collector.OnMotObject(MakeImage("d210_128x128.png", {6}));
  EXPECT_EQ(handler.logos.size(), 2u);
}

TEST(SpiCollector, ScheduleScopeFromMotParameters)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  MotObject object = MakeProgrammeInformation(Programme(1, "Nachrichten", SCOPE_START, 300));
  object.parameters[0x25] = Time(SCOPE_START);
  object.parameters[0x26] = Time(SCOPE_START + 86400);
  object.parameters[0x27] = Bearer(0xE0, 0x10BC, 0xD210);
  collector.OnMotObject(object);

  ASSERT_EQ(handler.schedules.size(), 1u);
  const SpiSchedule& schedule = handler.schedules[0];
  EXPECT_EQ(schedule.scopeStart, SCOPE_START);
  EXPECT_EQ(schedule.scopeEnd, SCOPE_START + 86400);
  ASSERT_EQ(schedule.services.size(), 1u);
  EXPECT_EQ(schedule.services[0].sid, 0xD210u);
  ASSERT_EQ(schedule.programmes.size(), 1u);
}

TEST(SpiCollector, ScopeOfDocumentTakesPrecedence)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  MotObject object = MakeProgrammeInformation(
      Concat({Scope(SCOPE_START, SCOPE_START + 3600, Bearer(0xE0, 0x10BC, 0xD220)),
              Programme(1, "Nachrichten", SCOPE_START, 300)}));
  object.parameters[0x25] = Time(SCOPE_START - 3600);
  object.parameters[0x26] = Time(SCOPE_START + 86400);
  object.parameters[0x27] = Bearer(0xE0, 0x10BC, 0xD210);
  collector.OnMotObject(object);

  ASSERT_EQ(handler.schedules.size(), 1u);
  EXPECT_EQ(handler.schedules[0].scopeStart, SCOPE_START);
  EXPECT_EQ(handler.schedules[0].scopeEnd, SCOPE_START + 3600);
  ASSERT_EQ(handler.schedules[0].services.size(), 1u);
  EXPECT_EQ(handler.schedules[0].services[0].sid, 0xD220u);
}

TEST(SpiCollector, OtherObjectsAreIgnored)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  collector.OnMotObject({.contentName = "journaline", .contentType = 5, .body = {1, 2, 3}});
  collector.OnMotObject(
      {.contentName = "broken.EHB", .contentType = 7, .contentSubType = 1, .body = {0x02, 0x10}});

  EXPECT_TRUE(handler.logos.empty());
  EXPECT_TRUE(handler.schedules.empty());
}

TEST(SpiCollector, LogoIsPassedOnAgainForChangedServices)
{
  CRecordingHandler handler;
  CSpiCollector collector(handler);
  collector.OnMotObject(MakeServiceInformation());
  collector.OnMotObject(MakeImage("d220_320x240.png", {4}));
  ASSERT_EQ(handler.logos.size(), 1u);

  MotObject changed = MakeServiceInformation();
  changed.body = ServiceInformation(
      {Service(Bearer(0xE0, 0x10BC, 0xD220), {Logo("d220_320x240.png", 320, 240)}),
       Service(Bearer(0xE0, 0x10BC, 0xD230), {Logo("d220_320x240.png", 320, 240)})});
  collector.OnMotObject(changed);
  collector.OnMotObject(MakeImage("d220_320x240.png", {4}));

  ASSERT_EQ(handler.logos.size(), 2u);
  ASSERT_EQ(handler.logos[1].first.size(), 2u);
  EXPECT_EQ(handler.logos[1].first[0].sid, 0xD220u);
  EXPECT_EQ(handler.logos[1].first[1].sid, 0xD230u);

  collector.OnMotObject(MakeImage("d220_320x240.png", {4}));
  EXPECT_EQ(handler.logos.size(), 2u);
}
