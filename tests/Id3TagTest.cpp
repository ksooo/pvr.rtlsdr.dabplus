/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/Id3Tag.h"

#include <map>
#include <string>

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

uint32_t ReadSynchsafe(const std::vector<uint8_t>& data, size_t offset)
{
  return (data[offset] << 21) | (data[offset + 1] << 14) | (data[offset + 2] << 7) |
         data[offset + 3];
}

// Frame id -> text, for a tag with UTF-8 text frames only
std::map<std::string, std::string> ParseTextFrames(const std::vector<uint8_t>& tag)
{
  std::map<std::string, std::string> frames;
  size_t offset = 10;
  while (offset + 10 <= tag.size())
  {
    const std::string id(tag.begin() + offset, tag.begin() + offset + 4);
    const uint32_t size = ReadSynchsafe(tag, offset + 4);
    EXPECT_EQ(tag[offset + 10], 0x03) << id << " is not UTF-8";
    frames[id] = std::string(tag.begin() + offset + 11, tag.begin() + offset + 10 + size);
    offset += 10 + size;
  }
  EXPECT_EQ(offset, tag.size());
  return frames;
}

} // unnamed namespace

TEST(Id3Tag, Header)
{
  const auto tag = CreateId3Tag({.title = "Jóga"});
  ASSERT_GE(tag.size(), 10u);
  EXPECT_EQ(std::string(tag.begin(), tag.begin() + 3), "ID3");
  EXPECT_EQ(tag[3], 4);
  EXPECT_EQ(tag[4], 0);
  EXPECT_EQ(tag[5], 0);
  EXPECT_EQ(ReadSynchsafe(tag, 6), tag.size() - 10);
}

TEST(Id3Tag, TextFrames)
{
  const auto frames = ParseTextFrames(CreateId3Tag(
      {.title = "Jóga", .artist = "Björk", .album = "Homogenic", .genre = "Pop Music"}));
  EXPECT_EQ(frames.at("TIT2"), "Jóga");
  EXPECT_EQ(frames.at("TPE1"), "Björk");
  EXPECT_EQ(frames.at("TALB"), "Homogenic");
  EXPECT_EQ(frames.at("TCON"), "Pop Music");
}

TEST(Id3Tag, EmptyFieldsAreWrittenAsEmptyFrames)
{
  const auto frames = ParseTextFrames(CreateId3Tag({.title = "Wissenschaft im Brennpunkt"}));
  ASSERT_EQ(frames.size(), 4u);
  EXPECT_EQ(frames.at("TPE1"), "");
  EXPECT_EQ(frames.at("TALB"), "");
}

TEST(Id3Tag, LongTextUsesSynchsafeSizes)
{
  const std::string title(300, 'x');
  const auto tag = CreateId3Tag({.title = title});
  EXPECT_EQ(ParseTextFrames(tag).at("TIT2"), title);
}

TEST(Id3Tag, PictureFrame)
{
  std::vector<uint8_t> image(300);
  for (size_t i = 0; i < image.size(); ++i)
    image[i] = static_cast<uint8_t>(i);

  const auto tag = CreateId3PictureTag("image/png", image);
  ASSERT_GE(tag.size(), 20u);
  EXPECT_EQ(std::string(tag.begin(), tag.begin() + 3), "ID3");
  EXPECT_EQ(ReadSynchsafe(tag, 6), tag.size() - 10);
  EXPECT_EQ(std::string(tag.begin() + 10, tag.begin() + 14), "APIC");
  EXPECT_EQ(ReadSynchsafe(tag, 14), tag.size() - 20);

  const std::vector<uint8_t> body(tag.begin() + 20, tag.end());
  const std::string header("\x00image/png\x00\x03\x00", 13);
  ASSERT_EQ(body.size(), header.size() + image.size());
  EXPECT_EQ(std::string(body.begin(), body.begin() + header.size()), header);
  EXPECT_EQ(std::vector<uint8_t>(body.begin() + header.size(), body.end()), image);
}
