/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiTestData.h"
#include "spi/SpiDecoder.h"

#include <string>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace SPI_TEST;

namespace
{

// 2026-10-03T22:00:00Z
constexpr std::time_t SCOPE_START = 1791064800;

} // unnamed namespace

TEST(SpiDecoder, BearerWithEnsemble)
{
  const auto id = DecodeSpiBearer(Bytes{0x40, 0xE0, 0x10, 0xBC, 0xD2, 0x10});
  ASSERT_TRUE(id);
  EXPECT_EQ(id->ecc, 0xE0);
  EXPECT_EQ(id->eid, 0x10BC);
  EXPECT_EQ(id->sid, 0xD210u);
  EXPECT_EQ(id->scids, 0);
  EXPECT_FALSE(id->isLongSid);
}

TEST(SpiDecoder, BearerWithoutEnsemble)
{
  const auto id = DecodeSpiBearer(Bytes{0x01, 0xD2, 0x20});
  ASSERT_TRUE(id);
  EXPECT_EQ(id->ecc, 0);
  EXPECT_EQ(id->sid, 0xD220u);
  EXPECT_EQ(id->scids, 1);
}

TEST(SpiDecoder, TruncatedBearer)
{
  EXPECT_FALSE(DecodeSpiBearer(Bytes{0x40, 0xE0, 0x10, 0xBC, 0xD2}));
  EXPECT_FALSE(DecodeSpiBearer(Bytes{}));
}

TEST(SpiDecoder, BearerMatchesService)
{
  const ServiceInfo service{.ecc = 0xE0, .sid = 0xD210, .scids = 0};
  EXPECT_TRUE((SpiServiceId{.ecc = 0xE0, .eid = 0x10BC, .sid = 0xD210}).Matches(service));
  EXPECT_TRUE((SpiServiceId{.sid = 0xD210}).Matches(service));
  EXPECT_FALSE((SpiServiceId{.ecc = 0xE1, .sid = 0xD210}).Matches(service));
  EXPECT_FALSE((SpiServiceId{.ecc = 0xE0, .sid = 0xD210, .scids = 1}).Matches(service));
  EXPECT_FALSE((SpiServiceId{.ecc = 0xE0, .sid = 0xD210, .isLongSid = true}).Matches(service));
}

TEST(SpiDecoder, ShortTimeWithLocalTimeOffset)
{
  // 2026-10-03T22:00Z, local time offset +2 h, as sent by Deutschlandradio
  EXPECT_EQ(DecodeSpiTime(Bytes{0x3B, 0xE1, 0x15, 0x80, 0x04}), SCOPE_START);
}

TEST(SpiDecoder, TimeWithSeconds)
{
  EXPECT_EQ(DecodeSpiTime(Bytes{0x3B, 0xE1, 0x0D, 0x85, 0x78, 0x00}), SCOPE_START + 5 * 60 + 30);
  EXPECT_EQ(DecodeSpiTime(Time(SCOPE_START + 4321)), SCOPE_START + 4321);
  EXPECT_FALSE(DecodeSpiTime(Bytes{0x3B, 0xE1, 0x0D, 0x85, 0x78}));
}

TEST(SpiDecoder, ServiceInformation)
{
  const Bytes square = Element(
      0x13, Element(0x2B, Concat({Element(0x82, Text("d210_32.png")), Element(0x83, {0x04})})));
  const Bytes rectangle = Element(
      0x13, Element(0x2B, Concat({Element(0x82, Text("d210_112.png")), Element(0x83, {0x06})})));
  const Bytes viaIp = Logo("https://example.com/logo.png", 600, 600);

  const auto services = DecodeServiceInformation(
      ServiceInformation({Service(Bearer(0xE0, 0x10BC, 0xD210),
                                  {square, rectangle, Logo("d210_128.png", 128, 128), viaIp}),
                          Service(Bearer(0xE0, 0x10BC, 0xD220), {})}));

  ASSERT_TRUE(services);
  ASSERT_EQ(services->size(), 2u);
  const SpiService& service = services->front();
  ASSERT_EQ(service.ids.size(), 1u);
  EXPECT_EQ(service.ids[0].sid, 0xD210u);
  ASSERT_EQ(service.logos.size(), 3u);
  EXPECT_EQ(service.logos[0].contentName, "d210_32.png");
  EXPECT_EQ(service.logos[0].width, 32);
  EXPECT_EQ(service.logos[1].contentName, "d210_112.png");
  EXPECT_EQ(service.logos[1].width, 112);
  EXPECT_EQ(service.logos[1].height, 32);
  EXPECT_EQ(service.logos[2].contentName, "d210_128.png");
  EXPECT_EQ(service.logos[2].height, 128);
  EXPECT_TRUE(services->back().logos.empty());
}

TEST(SpiDecoder, ProgrammeInformation)
{
  const std::string longDescription(400, 'x');
  const auto schedule = DecodeProgrammeInformation(ProgrammeInformation(
      Concat({Scope(SCOPE_START, SCOPE_START + 86400, Bearer(0xE0, 0x10BC, 0xD210)),
              Programme(0x154367, "Nachrichten", SCOPE_START, 300),
              Programme(0x154368, "Lange Nacht", SCOPE_START + 300, 6900, longDescription)})));

  ASSERT_TRUE(schedule);
  EXPECT_EQ(schedule->scopeStart, SCOPE_START);
  EXPECT_EQ(schedule->scopeEnd, SCOPE_START + 86400);
  ASSERT_EQ(schedule->services.size(), 1u);
  EXPECT_EQ(schedule->services[0].sid, 0xD210u);
  ASSERT_EQ(schedule->programmes.size(), 2u);

  const SpiProgramme& news = schedule->programmes[0];
  EXPECT_EQ(news.shortId, 0x154367u);
  EXPECT_EQ(news.title, "Nachrichten");
  EXPECT_EQ(news.start, SCOPE_START);
  EXPECT_EQ(news.duration, 300);
  EXPECT_TRUE(news.longDescription.empty());

  EXPECT_EQ(schedule->programmes[1].longDescription, longDescription);
}

TEST(SpiDecoder, ProgrammeDetails)
{
  const Bytes programme = Element(
      0x1C, Concat({Element(0x81, Uint24(7)), Element(0x10, Cdata("Short")),
                    Element(0x11, Cdata("Medium")), Element(0x12, Cdata("Long title")),
                    Element(0x19, Element(0x2C, Concat({Element(0x80, Time(SCOPE_START)),
                                                        Element(0x81, Uint16(60))}))),
                    Element(0x13, Element(0x1A, Cdata("Summary"))),
                    Element(0x14, Concat({Element(0x80, {0x03, 0x01}), Cdata("Nachrichten")}))}));
  const Bytes withoutTime =
      Element(0x1C, Concat({Element(0x81, Uint24(8)), Element(0x11, Cdata("No time"))}));

  const auto schedule =
      DecodeProgrammeInformation(ProgrammeInformation(Concat({programme, withoutTime})));

  ASSERT_TRUE(schedule);
  EXPECT_EQ(schedule->scopeStart, 0);
  EXPECT_TRUE(schedule->services.empty());
  ASSERT_EQ(schedule->programmes.size(), 1u);
  EXPECT_EQ(schedule->programmes[0].title, "Long title");
  EXPECT_EQ(schedule->programmes[0].shortDescription, "Summary");
  EXPECT_EQ(schedule->programmes[0].genre, "Nachrichten");
}

TEST(SpiDecoder, TokensAreReplaced)
{
  const Bytes tokenTable =
      Element(0x04, Concat({Bytes{0x01, 7}, Text("Morgen "), Bytes{0x0B, 4}, Text("Dlf ")}));
  const Bytes title = Concat({Bytes{0x0B}, Text("am "), Bytes{0x01}, Text("\n\t")});
  const Bytes document = Element(
      0x02,
      Concat({tokenTable,
              Element(0x21,
                      Element(0x1C,
                              Concat({Element(0x81, Uint24(1)), Element(0x11, Element(0x01, title)),
                                      Element(0x19, Element(0x2C, Concat({
                                                                      Element(0x80, Time(0)),
                                                                      Element(0x81, Uint16(1)),
                                                                  })))})))}));

  const auto schedule = DecodeProgrammeInformation(document);
  ASSERT_TRUE(schedule);
  ASSERT_EQ(schedule->programmes.size(), 1u);
  EXPECT_EQ(schedule->programmes[0].title, "Dlf am Morgen \n\t");
}

TEST(SpiDecoder, MalformedDocuments)
{
  const Bytes document =
      ProgrammeInformation(Programme(1, "Nachrichten", SCOPE_START, 300, "Text"));

  EXPECT_FALSE(DecodeProgrammeInformation(Bytes(document.begin(), document.end() - 1)));
  EXPECT_FALSE(DecodeProgrammeInformation(Bytes{}));
  EXPECT_FALSE(DecodeServiceInformation(document));
  EXPECT_FALSE(DecodeProgrammeInformation(ServiceInformation({})));
}
