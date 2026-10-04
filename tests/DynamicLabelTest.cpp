/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/DynamicLabel.h"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

struct Tag
{
  uint8_t contentType;
  uint8_t start;
  uint8_t length;
};

constexpr uint8_t TITLE = 1;
constexpr uint8_t ALBUM = 2;
constexpr uint8_t ARTIST = 4;

// A DL Plus tags command (ETSI TS 102 980, 7.3) in a single data group
std::vector<uint8_t> MakeDlPlusCommand(uint8_t link,
                                       const std::vector<Tag>& tags,
                                       uint8_t itemToggle = 0,
                                       bool itemRunning = true)
{
  std::vector<uint8_t> command{
      static_cast<uint8_t>((itemToggle << 3) | (itemRunning ? 0x04 : 0) | (tags.size() - 1))};
  for (const Tag& tag : tags)
  {
    command.push_back(tag.contentType);
    command.push_back(tag.start);
    command.push_back(static_cast<uint8_t>(tag.length - 1));
  }

  std::vector<uint8_t> dataGroup{0x70 | 0x02,
                                 static_cast<uint8_t>((link << 7) | (command.size() - 1))};
  dataGroup.insert(dataGroup.end(), command.begin(), command.end());
  return dataGroup;
}

} // unnamed namespace

TEST(DynamicLabel, LabelWithoutDlPlus)
{
  CDynamicLabelDecoder decoder;
  EXPECT_TRUE(decoder.ProcessLabel(0, "Wissenschaft im Brennpunkt"));
  EXPECT_FALSE(decoder.ProcessLabel(0, "Wissenschaft im Brennpunkt"));
  EXPECT_EQ(decoder.GetLabel().text, "Wissenschaft im Brennpunkt");
  EXPECT_FALSE(decoder.GetLabel().HasItem());
}

TEST(DynamicLabel, DlPlusTagsTitleAndArtist)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(1, "Now playing: Queen - Bohemian Rhapsody");

  EXPECT_TRUE(decoder.ProcessCommand(MakeDlPlusCommand(1, {{ARTIST, 13, 5}, {TITLE, 21, 17}})));
  EXPECT_EQ(decoder.GetLabel().artist, "Queen");
  EXPECT_EQ(decoder.GetLabel().title, "Bohemian Rhapsody");
  EXPECT_TRUE(decoder.GetLabel().album.empty());
}

TEST(DynamicLabel, DlPlusMarkersCountCharactersNotBytes)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Björk - Jóga");

  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 5}, {TITLE, 8, 4}}));
  EXPECT_EQ(decoder.GetLabel().artist, "Björk");
  EXPECT_EQ(decoder.GetLabel().title, "Jóga");
}

TEST(DynamicLabel, DlPlusTagBeyondLabelIsTruncated)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Album: Abbey Road");

  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ALBUM, 7, 30}}));
  EXPECT_EQ(decoder.GetLabel().album, "Abbey Road");
}

TEST(DynamicLabel, DlPlusTagsAreTrimmed)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "NDR 2 - ndr.de/ndr2");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{TITLE, 3, 1}, {ARTIST, 5, 1}}));
  EXPECT_FALSE(decoder.GetLabel().HasItem());

  decoder.ProcessLabel(1, "Now playing: Queen - Bohemian Rhapsody");
  decoder.ProcessCommand(MakeDlPlusCommand(1, {{ARTIST, 12, 7}}, 1));
  EXPECT_EQ(decoder.GetLabel().artist, "Queen");
}

TEST(DynamicLabel, DlPlusBeforeItsLabel)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Tina Turner - The Best");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 11}, {TITLE, 14, 8}}, 0));

  // The new label message is still being received
  EXPECT_FALSE(decoder.ProcessCommand(MakeDlPlusCommand(1, {{ARTIST, 0, 10}, {TITLE, 13, 13}}, 1)));
  EXPECT_EQ(decoder.GetLabel().artist, "Tina Turner");
  EXPECT_EQ(decoder.GetLabel().title, "The Best");

  EXPECT_TRUE(decoder.ProcessLabel(1, "Alphaville - Forever Young"));
  EXPECT_EQ(decoder.GetLabel().artist, "Alphaville");
  EXPECT_EQ(decoder.GetLabel().title, "Forever Young");
}

TEST(DynamicLabel, DlPlusAfterItsLabel)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Tina Turner - The Best");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 11}, {TITLE, 14, 8}}, 0));

  decoder.ProcessLabel(1, "Alphaville - Forever Young");
  EXPECT_TRUE(decoder.ProcessCommand(MakeDlPlusCommand(1, {{ARTIST, 0, 10}, {TITLE, 13, 13}}, 1)));
  EXPECT_EQ(decoder.GetLabel().artist, "Alphaville");
  EXPECT_EQ(decoder.GetLabel().title, "Forever Young");
}

TEST(DynamicLabel, PendingDlPlusOfOlderLabelIsDropped)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(1, "Queen - Bohemian Rhapsody");
  // Refers to a label message with toggle 0 that is never received completely
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 3}}, 1));
  decoder.ProcessLabel(1, "News");

  decoder.ProcessLabel(0, "Wonderwall");
  EXPECT_FALSE(decoder.GetLabel().HasItem());
}

TEST(DynamicLabel, ItemStaysWhileOtherMessagesAreShown)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 5}, {TITLE, 8, 17}}));

  decoder.ProcessLabel(1, "You are listening to Radio Bob!");
  EXPECT_EQ(decoder.GetLabel().text, "You are listening to Radio Bob!");
  EXPECT_EQ(decoder.GetLabel().artist, "Queen");
  EXPECT_EQ(decoder.GetLabel().title, "Bohemian Rhapsody");
}

TEST(DynamicLabel, NewItemReplacesAllItemFields)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 5}, {TITLE, 8, 17}}, 0));

  decoder.ProcessLabel(1, "Wonderwall");
  decoder.ProcessCommand(MakeDlPlusCommand(1, {{TITLE, 0, 10}}, 1));
  EXPECT_EQ(decoder.GetLabel().title, "Wonderwall");
  EXPECT_TRUE(decoder.GetLabel().artist.empty());
}

TEST(DynamicLabel, ItemNotRunningClearsItem)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 5}}));

  decoder.ProcessLabel(1, "The news at five");
  EXPECT_TRUE(decoder.ProcessCommand(MakeDlPlusCommand(1, {{TITLE, 0, 3}}, 0, false)));
  EXPECT_FALSE(decoder.GetLabel().HasItem());
  EXPECT_EQ(decoder.GetLabel().text, "The news at five");
}

TEST(DynamicLabel, ClearDisplay)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");
  decoder.ProcessCommand(MakeDlPlusCommand(0, {{ARTIST, 0, 5}}));

  const std::vector<uint8_t> clear{0x70 | 0x01, 0x00};
  EXPECT_TRUE(decoder.ProcessCommand(clear));
  EXPECT_EQ(decoder.GetLabel(), ProgrammeLabel{});
}

TEST(DynamicLabel, DlPlusCommandInSeveralSegments)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");

  const std::vector<uint8_t> whole = MakeDlPlusCommand(0, {{ARTIST, 0, 5}, {TITLE, 8, 17}});
  const std::vector<uint8_t> command(whole.begin() + 2, whole.end());

  // First segment: F=1, L=0; second segment: F=0, L=1, segment number 1
  std::vector<uint8_t> first{0x50 | 0x02, 0x03};
  first.insert(first.end(), command.begin(), command.begin() + 4);
  std::vector<uint8_t> second{0x30 | 0x02, static_cast<uint8_t>(0x10 | (command.size() - 4 - 1))};
  second.insert(second.end(), command.begin() + 4, command.end());

  EXPECT_FALSE(decoder.ProcessCommand(first));
  EXPECT_TRUE(decoder.ProcessCommand(second));
  EXPECT_EQ(decoder.GetLabel().artist, "Queen");
  EXPECT_EQ(decoder.GetLabel().title, "Bohemian Rhapsody");
}

TEST(DynamicLabel, SegmentWithoutFirstIsDropped)
{
  CDynamicLabelDecoder decoder;
  decoder.ProcessLabel(0, "Queen - Bohemian Rhapsody");

  const std::vector<uint8_t> second{0x30 | 0x02, 0x10, 0x00};
  EXPECT_FALSE(decoder.ProcessCommand(second));
  EXPECT_FALSE(decoder.GetLabel().HasItem());
}
