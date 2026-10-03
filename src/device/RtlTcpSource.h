/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "device/SampleSource.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace DABPLUS
{

class CTcpConnection;

/*!
 * \brief Receives samples from an rtl_tcp server. Stop() closes the connection, Open() it again
 * to restart streaming.
 */
class CRtlTcpSource : public ISampleSource
{
public:
  CRtlTcpSource(std::string host, uint16_t port, int ppm);
  ~CRtlTcpSource() override;

  bool IsAvailable() const override;
  bool Open() override;
  void Close() override;
  bool Start(SamplesCallback callback) override;
  void Stop() override;
  bool SetFrequency(uint32_t frequency) override;
  std::vector<int> GetGains() const override { return m_gains; }
  bool SetGain(int gain) override;
  std::string GetName() const override;

  /*!
   * \brief The gains of an RTL-SDR tuner type as reported in the rtl_tcp header. rtl_tcp only
   * transfers their number, librtlsdr defines the values.
   */
  static std::vector<int> GetTunerGains(uint32_t tunerType);

private:
  bool SendCommand(uint8_t command, uint32_t parameter);

  const std::string m_host;
  const uint16_t m_port{0};
  const int m_ppm{0};
  std::unique_ptr<CTcpConnection> m_connection;
  std::mutex m_sendMutex;
  std::vector<int> m_gains;
  std::atomic<bool> m_stop{false};
  std::thread m_thread;
};

} // namespace DABPLUS
