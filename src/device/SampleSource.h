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
enum class OpenResult
{
  OPENED,
  NOT_FOUND,
  //! Occupied by another program or, for rtl_tcp, another client
  IN_USE,
  //! No answer to the connection attempt, also what rtl_tcp does once its queue is full
  TIMED_OUT,
  FAILED
};

class ISampleSource
{
public:
  using SamplesCallback = std::function<void(std::span<const uint8_t> samples)>;

  virtual ~ISampleSource() = default;

  /*!
   * \brief Checks without occupying the device whether Open() would succeed. USB sticks in use
   * are reported as available, as that cannot be told without opening them.
   */
  virtual OpenResult Probe() const = 0;

  virtual OpenResult Open() = 0;
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
