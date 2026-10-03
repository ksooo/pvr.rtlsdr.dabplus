/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "RtlTcpSource.h"

#include "utils/Log.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <exception>
#include <optional>
#include <utility>

#include <kissnet.hpp>

namespace DABPLUS
{

class CTcpConnection
{
public:
  explicit CTcpConnection(const kissnet::endpoint& endpoint) : m_socket(endpoint) {}

  kissnet::tcp_socket m_socket;
};

namespace
{

constexpr int64_t CONNECT_TIMEOUT_MS = 5000;
// A server busy with another client accepts the connection but does not send the header
constexpr int64_t HEADER_TIMEOUT_MS = 3000;
constexpr size_t RECEIVE_BUFFER_SIZE = 65536;

// rtl_tcp command codes
constexpr uint8_t CMD_SET_FREQUENCY = 0x01;
constexpr uint8_t CMD_SET_SAMPLE_RATE = 0x02;
constexpr uint8_t CMD_SET_GAIN_MODE = 0x03;
constexpr uint8_t CMD_SET_GAIN = 0x04;
constexpr uint8_t CMD_SET_FREQUENCY_CORRECTION = 0x05;
constexpr uint8_t CMD_SET_AGC_MODE = 0x08;

constexpr uint32_t TUNER_E4000 = 1;
constexpr uint32_t TUNER_FC0012 = 2;
constexpr uint32_t TUNER_FC0013 = 3;
constexpr uint32_t TUNER_R820T = 5;
constexpr uint32_t TUNER_R828D = 6;

uint32_t ReadBigEndian32(const std::byte* data)
{
  return (std::to_integer<uint32_t>(data[0]) << 24) | (std::to_integer<uint32_t>(data[1]) << 16) |
         (std::to_integer<uint32_t>(data[2]) << 8) | std::to_integer<uint32_t>(data[3]);
}

struct ServerConnection
{
  std::unique_ptr<CTcpConnection> connection;
  uint32_t tunerType{0};
};

std::optional<ServerConnection> ConnectToServer(const std::string& host,
                                                uint16_t port,
                                                LogLevel errorLevel)
{
  try
  {
    auto connection = std::make_unique<CTcpConnection>(kissnet::endpoint(host, port));
    if (connection->m_socket.connect(CONNECT_TIMEOUT_MS) != kissnet::socket_status::valid)
    {
      Log(errorLevel, "Unable to connect to rtl_tcp server {}:{}", host, port);
      return {};
    }

    if (connection->m_socket.select(kissnet::fds_read, HEADER_TIMEOUT_MS) !=
        kissnet::socket_status::valid)
    {
      Log(errorLevel, "rtl_tcp server {}:{} does not respond, it may be in use", host, port);
      return {};
    }

    // Header: "RTL0", tuner type, number of gain steps; all integers big endian
    std::array<std::byte, 12> header{};
    const auto [received, status] = connection->m_socket.recv(header.data(), header.size());
    if (status != kissnet::socket_status::valid || received != header.size() ||
        std::memcmp(header.data(), "RTL0", 4) != 0)
    {
      Log(errorLevel, "{}:{} is not an rtl_tcp server", host, port);
      return {};
    }

    return ServerConnection{std::move(connection), ReadBigEndian32(header.data() + 4)};
  }
  catch (const std::exception& e)
  {
    Log(errorLevel, "Unable to connect to rtl_tcp server {}:{}: {}", host, port, e.what());
    return {};
  }
}

} // unnamed namespace

CRtlTcpSource::CRtlTcpSource(std::string host, uint16_t port, int ppm)
  : m_host(std::move(host)),
    m_port(port),
    m_ppm(ppm)
{
}

CRtlTcpSource::~CRtlTcpSource()
{
  Close();
}

std::vector<int> CRtlTcpSource::GetTunerGains(uint32_t tunerType)
{
  switch (tunerType)
  {
    case TUNER_E4000:
      return {-10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420};
    case TUNER_FC0012:
      return {-99, -40, 71, 179, 192};
    case TUNER_FC0013:
      return {-99, -73, -65, -63, -60, -58, -54, 58,  61,  63,  65, 67,
              68,  70,  71,  179, 181, 182, 184, 186, 188, 191, 197};
    case TUNER_R820T:
    case TUNER_R828D:
      return {0,   9,   14,  27,  37,  77,  87,  125, 144, 157, 166, 197, 207, 229, 254,
              280, 297, 328, 338, 364, 372, 386, 402, 421, 434, 439, 445, 480, 496};
    default:
      return {};
  }
}

bool CRtlTcpSource::IsAvailable() const
{
  auto server = ConnectToServer(m_host, m_port, LogLevel::LEVEL_DEBUG);
  if (!server)
    return false;

  server->connection->m_socket.close();
  return true;
}

bool CRtlTcpSource::Open()
{
  if (m_connection)
    return true;

  auto server = ConnectToServer(m_host, m_port, LogLevel::LEVEL_ERROR);
  if (!server)
    return false;

  m_gains = GetTunerGains(server->tunerType);
  m_connection = std::move(server->connection);
  Log(LogLevel::LEVEL_INFO, "Connected to rtl_tcp server {}:{}, tuner type {}", m_host, m_port,
      server->tunerType);

  SendCommand(CMD_SET_SAMPLE_RATE, DAB_SAMPLE_RATE);
  SendCommand(CMD_SET_GAIN_MODE, 1);
  SendCommand(CMD_SET_AGC_MODE, 0);
  if (m_ppm != 0)
    SendCommand(CMD_SET_FREQUENCY_CORRECTION, static_cast<uint32_t>(m_ppm));

  return true;
}

void CRtlTcpSource::Close()
{
  Stop();

  if (m_connection)
  {
    m_connection->m_socket.close();
    m_connection.reset();
  }
}

bool CRtlTcpSource::Start(SamplesCallback callback)
{
  if (!m_connection || m_thread.joinable())
    return false;

  m_stop = false;
  m_thread = std::thread(
      [this, callback = std::move(callback)]
      {
        std::vector<std::byte> buffer(RECEIVE_BUFFER_SIZE);
        size_t filled{0};
        while (!m_stop)
        {
          const auto [received, status] =
              m_connection->m_socket.recv(buffer.data() + filled, buffer.size() - filled);
          if (status != kissnet::socket_status::valid)
          {
            if (!m_stop)
              Log(LogLevel::LEVEL_ERROR, "Connection to rtl_tcp server {}:{} lost", m_host, m_port);
            break;
          }

          // Only pass complete I/Q pairs on, keep a dangling I sample for the next round
          filled += received;
          const size_t usable = filled & ~size_t{1};
          callback({reinterpret_cast<const uint8_t*>(buffer.data()), usable});
          if (usable < filled)
            buffer[0] = buffer[usable];
          filled -= usable;
        }
      });
  return true;
}

void CRtlTcpSource::Stop()
{
  if (!m_thread.joinable())
    return;

  // Unblocks the receiving thread; the connection cannot stream again afterwards
  m_stop = true;
  m_connection->m_socket.shutdown();
  m_thread.join();
  m_connection->m_socket.close();
  m_connection.reset();
}

bool CRtlTcpSource::SetFrequency(uint32_t frequency)
{
  return SendCommand(CMD_SET_FREQUENCY, frequency);
}

bool CRtlTcpSource::SetGain(int gain)
{
  return SendCommand(CMD_SET_GAIN, static_cast<uint32_t>(gain));
}

std::string CRtlTcpSource::GetName() const
{
  return fmt::format("rtl_tcp {}:{}", m_host, m_port);
}

bool CRtlTcpSource::SendCommand(uint8_t command, uint32_t parameter)
{
  if (!m_connection)
    return false;

  const std::array<std::byte, 5> message{std::byte{command}, std::byte(parameter >> 24),
                                         std::byte(parameter >> 16), std::byte(parameter >> 8),
                                         std::byte(parameter)};

  std::lock_guard<std::mutex> lock(m_sendMutex);
  const auto [sent, status] = m_connection->m_socket.send(message.data(), message.size());
  return status == kissnet::socket_status::valid && sent == message.size();
}

} // namespace DABPLUS
