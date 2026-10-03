/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "LiveStream.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace DABPLUS
{

namespace
{

constexpr int64_t MICROSECONDS_PER_SECOND = 1000000;
constexpr int64_t MAX_QUEUED_AUDIO_SECONDS = 3;

} // unnamed namespace

CLiveStream::CLiveStream(std::string genre) : m_genre(std::move(genre))
{
}

void CLiveStream::OnAudio(const AudioFormat& format, std::span<const int16_t> samples)
{
  if (format.sampleRate == 0 || format.channels == 0 || samples.empty())
    return;

  StreamPacket packet;
  packet.type = StreamPacket::Type::AUDIO;
  packet.data.resize(samples.size_bytes());
  std::memcpy(packet.data.data(), samples.data(), samples.size_bytes());

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_format != format)
    {
      m_format = format;
      m_formatStartPts = m_nextPts;
      m_formatFrames = 0;
      m_packets.push_back({StreamPacket::Type::FORMAT_CHANGE, m_nextPts, 0, {}});
    }

    // Derived from the total number of frames, so that rounding errors do not accumulate
    m_formatFrames += static_cast<int64_t>(samples.size() / format.channels);
    packet.pts = m_nextPts;
    m_nextPts = m_formatStartPts + m_formatFrames * MICROSECONDS_PER_SECOND / format.sampleRate;
    packet.duration = m_nextPts - packet.pts;
    m_queuedAudioBytes += packet.data.size();
    m_packets.push_back(std::move(packet));

    const size_t maxQueuedBytes =
        MAX_QUEUED_AUDIO_SECONDS * format.sampleRate * format.channels * sizeof(int16_t);
    while (m_queuedAudioBytes > maxQueuedBytes)
    {
      const auto oldest =
          std::ranges::find(m_packets, StreamPacket::Type::AUDIO, &StreamPacket::type);
      m_queuedAudioBytes -= oldest->data.size();
      m_packets.erase(oldest);
    }
  }
  m_condition.notify_all();
}

void CLiveStream::OnLabel(const ProgrammeLabel& label)
{
  Id3Fields fields;
  fields.title = label.HasItem() ? label.title : label.text;
  fields.artist = label.artist;
  fields.album = label.album;
  fields.genre = m_genre;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_lastFields == fields)
      return;

    m_lastFields = fields;
    m_packets.push_back({StreamPacket::Type::METADATA, m_nextPts, 0, CreateId3Tag(fields)});
  }
  m_condition.notify_all();
}

std::optional<AudioFormat> CLiveStream::WaitForAudio(std::chrono::milliseconds timeout)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_condition.wait_for(lock, timeout, [this] { return m_aborted || m_format.has_value(); });
  return m_format;
}

std::optional<AudioFormat> CLiveStream::GetAudioFormat() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_format;
}

std::optional<StreamPacket> CLiveStream::Read(std::chrono::milliseconds timeout)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  if (!m_condition.wait_for(lock, timeout, [this] { return m_aborted || !m_packets.empty(); }) ||
      m_aborted)
    return {};

  StreamPacket packet = std::move(m_packets.front());
  m_packets.pop_front();
  if (packet.type == StreamPacket::Type::AUDIO)
    m_queuedAudioBytes -= packet.data.size();
  return packet;
}

void CLiveStream::Abort()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aborted = true;
  }
  m_condition.notify_all();
}

void CLiveStream::Flush()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  std::erase_if(m_packets, [](const StreamPacket& packet)
                { return packet.type == StreamPacket::Type::AUDIO; });
  m_queuedAudioBytes = 0;
}

} // namespace DABPLUS
