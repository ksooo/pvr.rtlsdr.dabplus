/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "stream/LiveStream.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace std::chrono_literals;

namespace
{

constexpr AudioFormat STEREO_48K{48000, 2};

std::vector<int16_t> MakeSamples(size_t frames, uint8_t channels)
{
  return std::vector<int16_t>(frames * channels, 1000);
}

bool Contains(const std::vector<uint8_t>& data, const std::string& text)
{
  return std::search(data.begin(), data.end(), text.begin(), text.end()) != data.end();
}

} // unnamed namespace

TEST(LiveStream, FirstAudioIsPrecededByFormatChange)
{
  CLiveStream stream("");
  stream.OnAudio(STEREO_48K, MakeSamples(1152, 2));

  auto packet = stream.Read(0ms);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->type, StreamPacket::Type::FORMAT_CHANGE);

  packet = stream.Read(0ms);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->type, StreamPacket::Type::AUDIO);
  EXPECT_EQ(packet->pts, 0);
  EXPECT_EQ(packet->duration, 24000);
  EXPECT_EQ(packet->data.size(), 1152u * 2 * sizeof(int16_t));
  EXPECT_FALSE(stream.Read(0ms));
}

TEST(LiveStream, TimestampsDoNotDrift)
{
  CLiveStream stream("");
  constexpr AudioFormat format{44100, 2};

  int64_t expectedPts{0};
  int64_t lastPts{0};
  int64_t lastDuration{0};
  for (int i = 0; i < 441; ++i)
  {
    stream.OnAudio(format, MakeSamples(1000, 2));
    while (const auto packet = stream.Read(0ms))
    {
      if (packet->type != StreamPacket::Type::AUDIO)
        continue;
      EXPECT_EQ(packet->pts, expectedPts);
      expectedPts += packet->duration;
      lastPts = packet->pts;
      lastDuration = packet->duration;
    }
  }

  // 441000 frames at 44.1 kHz are exactly ten seconds
  EXPECT_EQ(lastPts + lastDuration, 10000000);
}

TEST(LiveStream, FormatChangeContinuesTimestamps)
{
  CLiveStream stream("");
  stream.OnAudio(STEREO_48K, MakeSamples(48000, 2));
  stream.OnAudio({32000, 2}, MakeSamples(32000, 2));

  std::vector<StreamPacket> packets;
  while (auto packet = stream.Read(0ms))
    packets.emplace_back(std::move(*packet));

  ASSERT_EQ(packets.size(), 4u);
  EXPECT_EQ(packets[2].type, StreamPacket::Type::FORMAT_CHANGE);
  EXPECT_EQ(packets[3].pts, 1000000);
  EXPECT_EQ(packets[3].duration, 1000000);
  EXPECT_EQ(stream.GetAudioFormat(), (AudioFormat{32000, 2}));
}

TEST(LiveStream, LabelWithoutItemBecomesTitle)
{
  CLiveStream stream("Information");
  stream.OnLabel({.text = "Wissenschaft im Brennpunkt"});

  const auto packet = stream.Read(0ms);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->type, StreamPacket::Type::METADATA);
  EXPECT_TRUE(Contains(packet->data, "Wissenschaft im Brennpunkt"));
  EXPECT_TRUE(Contains(packet->data, "Information"));
}

TEST(LiveStream, ItemTakesPrecedenceOverLabelText)
{
  CLiveStream stream("");
  stream.OnLabel({.text = "Now playing: Queen - Bohemian Rhapsody",
                  .title = "Bohemian Rhapsody",
                  .artist = "Queen"});

  const auto packet = stream.Read(0ms);
  ASSERT_TRUE(packet);
  EXPECT_FALSE(Contains(packet->data, "Now playing"));
  EXPECT_TRUE(Contains(packet->data, "Bohemian Rhapsody"));
  EXPECT_TRUE(Contains(packet->data, "Queen"));
}

TEST(LiveStream, UnchangedMetadataIsSentOnce)
{
  CLiveStream stream("");
  stream.OnLabel({.text = "Queen - Bohemian Rhapsody", .title = "Bohemian Rhapsody"});
  stream.OnLabel({.text = "You are listening to Radio Bob!", .title = "Bohemian Rhapsody"});

  EXPECT_TRUE(stream.Read(0ms));
  EXPECT_FALSE(stream.Read(0ms));
}

TEST(LiveStream, WaitForAudio)
{
  CLiveStream stream("");
  EXPECT_FALSE(stream.WaitForAudio(10ms));

  std::thread producer([&stream] { stream.OnAudio(STEREO_48K, MakeSamples(1152, 2)); });
  EXPECT_EQ(stream.WaitForAudio(2s), STEREO_48K);
  producer.join();
}

TEST(LiveStream, AbortWakesReader)
{
  CLiveStream stream("");
  std::thread aborter(
      [&stream]
      {
        std::this_thread::sleep_for(20ms);
        stream.Abort();
      });
  const auto start = std::chrono::steady_clock::now();
  EXPECT_FALSE(stream.Read(5s));
  EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
  aborter.join();
}

TEST(LiveStream, OldestAudioIsDroppedWhenReaderFallsBehind)
{
  CLiveStream stream("");
  stream.OnLabel({.text = "label"});
  for (int i = 0; i < 5; ++i)
    stream.OnAudio(STEREO_48K, MakeSamples(48000, 2));

  std::vector<StreamPacket> audio;
  bool hasMetadata{false};
  while (auto packet = stream.Read(0ms))
  {
    if (packet->type == StreamPacket::Type::AUDIO)
      audio.emplace_back(std::move(*packet));
    hasMetadata = hasMetadata || packet->type == StreamPacket::Type::METADATA;
  }

  EXPECT_TRUE(hasMetadata);
  ASSERT_EQ(audio.size(), 3u);
  EXPECT_EQ(audio.front().pts, 2000000);
}

TEST(LiveStream, FlushDropsAudio)
{
  CLiveStream stream("");
  stream.OnAudio(STEREO_48K, MakeSamples(1152, 2));
  stream.Flush();

  const auto packet = stream.Read(0ms);
  ASSERT_TRUE(packet);
  EXPECT_EQ(packet->type, StreamPacket::Type::FORMAT_CHANGE);
  EXPECT_FALSE(stream.Read(0ms));
}
