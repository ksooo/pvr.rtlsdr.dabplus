/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtlSdrUsbSource.h"

#include "utils/Log.h"

#include <array>
#include <chrono>
#include <utility>

#include <rtl-sdr.h>

namespace DABPLUS
{

namespace
{

constexpr uint32_t ASYNC_BUFFER_COUNT = 16;
constexpr uint32_t ASYNC_BUFFER_SIZE = 65536;

// What rtlsdr_open() passes on from libusb when another program has claimed the device
constexpr int LIBUSB_ERROR_BUSY = -6;

// Cheap sticks often share the same serial number, so several devices may match
std::vector<uint32_t> FindDevices(const std::string& serial)
{
  std::vector<uint32_t> indices;
  const uint32_t count = rtlsdr_get_device_count();
  for (uint32_t index = 0; index < count; ++index)
  {
    std::array<char, 256> manufacturer{};
    std::array<char, 256> product{};
    std::array<char, 256> deviceSerial{};
    if (rtlsdr_get_device_usb_strings(index, manufacturer.data(), product.data(),
                                      deviceSerial.data()) != 0)
      continue;

    if (serial.empty() || serial == deviceSerial.data())
      indices.emplace_back(index);
  }
  return indices;
}

} // unnamed namespace

CRtlSdrUsbSource::CRtlSdrUsbSource(std::string serial, int ppm)
  : m_serial(std::move(serial)),
    m_ppm(ppm)
{
}

CRtlSdrUsbSource::~CRtlSdrUsbSource()
{
  Close();
}

OpenResult CRtlSdrUsbSource::Probe() const
{
  return FindDevices(m_serial).empty() ? OpenResult::NOT_FOUND : OpenResult::OPENED;
}

OpenResult CRtlSdrUsbSource::Open()
{
  if (m_device)
    return OpenResult::OPENED;

  const std::vector<uint32_t> indices = FindDevices(m_serial);
  if (indices.empty())
  {
    Log(LogLevel::LEVEL_ERROR, "No RTL-SDR USB device{} found",
        m_serial.empty() ? "" : fmt::format(" with serial number '{}'", m_serial));
    return OpenResult::NOT_FOUND;
  }

  // Take the first matching device that is not in use, e.g. by another add-on instance
  bool isInUse{false};
  for (const uint32_t index : indices)
  {
    const int result = rtlsdr_open(&m_device, index);
    if (result == 0)
      break;

    isInUse = isInUse || result == LIBUSB_ERROR_BUSY;
    Log(LogLevel::LEVEL_DEBUG, "Unable to open RTL-SDR USB device {}, error {}", index, result);
    m_device = nullptr;
  }

  if (!m_device)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to open any matching RTL-SDR USB device");
    return isInUse ? OpenResult::IN_USE : OpenResult::FAILED;
  }

  std::array<char, 256> manufacturer{};
  std::array<char, 256> product{};
  std::array<char, 256> serial{};
  rtlsdr_get_usb_strings(m_device, manufacturer.data(), product.data(), serial.data());
  m_name = fmt::format("{} {} (serial {})", manufacturer.data(), product.data(), serial.data());

  rtlsdr_set_sample_rate(m_device, DAB_SAMPLE_RATE);
  if (m_ppm != 0)
    rtlsdr_set_freq_correction(m_device, m_ppm);

  // The tuner's AGC overdrives the ADC on strong DAB signals, the receiver controls the gain.
  rtlsdr_set_tuner_gain_mode(m_device, 1);
  rtlsdr_set_agc_mode(m_device, 0);

  const int gainCount = rtlsdr_get_tuner_gains(m_device, nullptr);
  if (gainCount > 0)
  {
    m_gains.resize(static_cast<size_t>(gainCount));
    rtlsdr_get_tuner_gains(m_device, m_gains.data());
  }

  Log(LogLevel::LEVEL_INFO, "Opened {}, tuner type {}, {} gain steps", m_name,
      static_cast<int>(rtlsdr_get_tuner_type(m_device)), m_gains.size());
  return OpenResult::OPENED;
}

void CRtlSdrUsbSource::Close()
{
  Stop();

  if (m_device)
  {
    rtlsdr_close(m_device);
    m_device = nullptr;
  }
}

bool CRtlSdrUsbSource::Start(SamplesCallback callback)
{
  if (!m_device || m_thread.joinable())
    return false;

  m_callback = std::move(callback);
  rtlsdr_reset_buffer(m_device);
  m_isReading = true;
  m_thread = std::thread(
      [this]
      {
        const int result = rtlsdr_read_async(
            m_device,
            [](unsigned char* buffer, uint32_t length, void* context)
            {
              auto* self = static_cast<CRtlSdrUsbSource*>(context);
              self->m_callback({buffer, length});
            },
            this, ASYNC_BUFFER_COUNT, ASYNC_BUFFER_SIZE);
        m_isReading = false;
        if (result != 0)
          Log(LogLevel::LEVEL_ERROR, "Reading from RTL-SDR USB device failed ({})", result);
      });
  return true;
}

void CRtlSdrUsbSource::Stop()
{
  if (!m_thread.joinable())
    return;

  // Cancelling has no effect until rtlsdr_read_async() has set up its transfers
  while (m_isReading)
  {
    rtlsdr_cancel_async(m_device);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  m_thread.join();
}

bool CRtlSdrUsbSource::SetFrequency(uint32_t frequency)
{
  return m_device && rtlsdr_set_center_freq(m_device, frequency) == 0;
}

bool CRtlSdrUsbSource::SetGain(int gain)
{
  return m_device && rtlsdr_set_tuner_gain(m_device, gain) == 0;
}

} // namespace DABPLUS
