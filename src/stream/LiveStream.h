/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceListener.h"
#include "stream/Id3Tag.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace DABPLUS
{

struct StreamPacket
{
  enum class Type
  {
    //! The audio format changed, the following audio packets use the new format
    FORMAT_CHANGE,
    //! Interleaved signed 16 bit little endian samples
    AUDIO,
    //! An ID3v2 tag
    METADATA,
  };

  Type type{Type::AUDIO};
  int64_t pts{0}; // microseconds
  int64_t duration{0}; // microseconds
  std::vector<uint8_t> data;
};

/*!
 * \brief Turns the audio, labels and slideshow of a service into a sequence of packets for the
 * player.
 *
 * Timestamps advance with the number of samples received. If the player does not keep up, the
 * oldest audio is dropped.
 */
class CLiveStream : public IServiceListener
{
public:
  /*!
   * \param genre sent as genre with each programme label
   * \param lead audio held back before the first packet is passed on, see Read()
   */
  explicit CLiveStream(std::string genre, std::chrono::milliseconds lead = {});

  void OnAudio(const AudioFormat& format, std::span<const int16_t> samples) override;
  void OnLabel(const ProgrammeLabel& label) override;
  void OnPicture(std::string_view mimeType, std::span<const uint8_t> image) override;

  /*!
   * \return the format of the audio, once audio arrived within the timeout
   */
  std::optional<AudioFormat> WaitForAudio(std::chrono::milliseconds timeout);

  std::optional<AudioFormat> GetAudioFormat() const;

  /*!
   * \brief Passes nothing on until audio of the lead time is queued, after the start and after
   * Flush(), so that the player has audio to bridge irregular arrival with.
   *
   * \return the next packet, or nothing if none is available within the timeout or after Abort()
   */
  std::optional<StreamPacket> Read(std::chrono::milliseconds timeout);

  void Abort();
  void Flush();

private:
  bool IsReadable();

  const std::string m_genre;
  const std::chrono::milliseconds m_lead;

  mutable std::mutex m_mutex;
  std::condition_variable m_condition;
  std::deque<StreamPacket> m_packets;
  size_t m_queuedAudioBytes{0};
  std::optional<AudioFormat> m_format;
  int64_t m_nextPts{0};
  int64_t m_formatStartPts{0};
  int64_t m_formatFrames{0};
  std::optional<Id3Fields> m_lastFields;
  bool m_isHoldingBack;
  bool m_aborted{false};
};

} // namespace DABPLUS
