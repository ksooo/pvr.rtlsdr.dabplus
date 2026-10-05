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

struct rtlsdr_dev;

namespace DABPLUS
{

class CRtlSdrUsbSource : public ISampleSource
{
public:
  /*!
   * \param serial serial number of the device to open; empty for the first device found
   * \param ppm frequency correction of the device's crystal
   */
  CRtlSdrUsbSource(std::string serial, int ppm);
  ~CRtlSdrUsbSource() override;

  OpenResult Probe() const override;
  OpenResult Open() override;
  void Close() override;
  bool Start(SamplesCallback callback) override;
  void Stop() override;
  bool SetFrequency(uint32_t frequency) override;
  std::vector<int> GetGains() const override { return m_gains; }
  bool SetGain(int gain) override;
  std::string GetName() const override { return m_name; }

private:
  const std::string m_serial;
  const int m_ppm{0};
  rtlsdr_dev* m_device{nullptr};
  std::string m_name;
  std::vector<int> m_gains;
  SamplesCallback m_callback;
  std::atomic<bool> m_isReading{false};
  std::thread m_thread;
};

} // namespace DABPLUS
