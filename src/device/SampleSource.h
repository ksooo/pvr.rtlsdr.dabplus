/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace DABPLUS
{

constexpr uint32_t DAB_SAMPLE_RATE = 2048000;

/*!
 * \brief A source of interleaved unsigned 8 bit I/Q samples at DAB_SAMPLE_RATE, as delivered by
 * RTL-SDR devices.
 */
class ISampleSource
{
public:
  using SamplesCallback = std::function<void(std::span<const uint8_t> samples)>;

  virtual ~ISampleSource() = default;

  /*!
   * \brief Whether the device appears to be usable, checked without occupying it.
   */
  virtual bool IsAvailable() const = 0;

  virtual bool Open() = 0;
  virtual void Close() = 0;

  /*!
   * \brief Starts streaming. The callback is invoked on a thread owned by the source.
   */
  virtual bool Start(SamplesCallback callback) = 0;
  virtual void Stop() = 0;

  virtual bool SetFrequency(uint32_t frequency) = 0;

  /*!
   * \brief The tuner gains in 1/10 dB, in ascending order. Empty if the gain cannot be set.
   */
  virtual std::vector<int> GetGains() const = 0;
  virtual bool SetGain(int gain) = 0;

  /*!
   * \brief A human readable description of the device, for logging and the PVR backend info.
   */
  virtual std::string GetName() const = 0;
};

} // namespace DABPLUS
