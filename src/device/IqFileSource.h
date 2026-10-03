/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "device/SampleSource.h"

#include <atomic>
#include <thread>

namespace DABPLUS
{

/*!
 * \brief Plays back a raw recording of interleaved unsigned 8 bit I/Q samples (as written by
 * rtl_sdr) in real time and in a loop. The frequency is ignored.
 */
class CIqFileSource : public ISampleSource
{
public:
  explicit CIqFileSource(std::string path);
  ~CIqFileSource() override;

  bool IsAvailable() const override;
  bool Open() override;
  void Close() override;
  bool Start(SamplesCallback callback) override;
  void Stop() override;
  bool SetFrequency(uint32_t frequency) override { return true; }
  std::vector<int> GetGains() const override { return {}; }
  bool SetGain(int gain) override { return false; }
  std::string GetName() const override { return m_path; }

private:
  const std::string m_path;
  std::vector<uint8_t> m_samples;
  std::atomic<bool> m_stop{false};
  std::thread m_thread;
};

} // namespace DABPLUS
