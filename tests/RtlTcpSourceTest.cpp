/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "device/RtlTcpSource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <kissnet.hpp>

using namespace DABPLUS;
using namespace std::chrono_literals;

namespace
{

constexpr uint32_t TUNER_R820T = 5;

struct Command
{
  uint8_t code{0};
  uint32_t parameter{0};
  bool operator==(const Command& other) const = default;
};

/*!
 * \brief Accepts one client, sends the rtl_tcp header and records the commands received. Without
 * magic, it behaves like a server busy with another client and sends nothing.
 */
class CFakeRtlTcpServer
{
public:
  explicit CFakeRtlTcpServer(std::string_view magic = "RTL0") : m_magic(magic)
  {
    for (uint16_t port = 47100; port < 47200 && !m_listener; ++port)
    {
      try
      {
        auto listener = std::make_unique<kissnet::tcp_socket>(kissnet::endpoint("127.0.0.1", port));
        listener->bind();
        listener->listen();
        m_listener = std::move(listener);
        m_port = port;
      }
      catch (const std::exception&)
      {
      }
    }

    m_thread = std::thread([this] { Serve(); });
  }

  ~CFakeRtlTcpServer()
  {
    if (m_listener)
      m_listener->shutdown();
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_client)
        m_client->shutdown();
    }
    m_thread.join();
  }

  uint16_t GetPort() const { return m_port; }

  bool WaitForCommand(const Command& command)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_condition.wait_for(
        lock, 2s, [&] { return std::ranges::find(m_commands, command) != m_commands.end(); });
  }

  void Send(const std::vector<uint8_t>& data)
  {
    kissnet::tcp_socket* client{nullptr};
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition.wait_for(lock, 2s, [this] { return m_client != nullptr; });
      client = m_client.get();
    }
    ASSERT_NE(client, nullptr);
    client->send(reinterpret_cast<const std::byte*>(data.data()), data.size());
  }

private:
  void Serve()
  {
    if (!m_listener)
      return;

    try
    {
      auto client = std::make_unique<kissnet::tcp_socket>(m_listener->accept());

      std::array<std::byte, 12> header{};
      std::memcpy(header.data(), m_magic.data(), m_magic.size());
      header[7] = std::byte{TUNER_R820T};
      header[11] = std::byte{29};
      if (!m_magic.empty())
        client->send(header.data(), header.size());

      kissnet::tcp_socket* socket = client.get();
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_client = std::move(client);
      }
      m_condition.notify_all();

      std::array<std::byte, 5> message{};
      while (true)
      {
        const auto [received, status] = socket->recv(message.data(), message.size());
        if (status != kissnet::socket_status::valid || received != message.size())
          break;

        const Command command{std::to_integer<uint8_t>(message[0]),
                              (std::to_integer<uint32_t>(message[1]) << 24) |
                                  (std::to_integer<uint32_t>(message[2]) << 16) |
                                  (std::to_integer<uint32_t>(message[3]) << 8) |
                                  std::to_integer<uint32_t>(message[4])};
        std::lock_guard<std::mutex> lock(m_mutex);
        m_commands.push_back(command);
        m_condition.notify_all();
      }
    }
    catch (const std::exception&)
    {
    }
  }

  const std::string_view m_magic;
  std::unique_ptr<kissnet::tcp_socket> m_listener;
  uint16_t m_port{0};
  std::thread m_thread;

  std::mutex m_mutex;
  std::condition_variable m_condition;
  std::unique_ptr<kissnet::tcp_socket> m_client;
  std::vector<Command> m_commands;
};

} // unnamed namespace

TEST(RtlTcpSource, OpenReadsHeaderAndConfiguresTuner)
{
  CFakeRtlTcpServer server;
  ASSERT_NE(server.GetPort(), 0);

  CRtlTcpSource source("127.0.0.1", server.GetPort(), -3);
  ASSERT_TRUE(source.Open());

  EXPECT_EQ(source.GetGains(), CRtlTcpSource::GetTunerGains(TUNER_R820T));
  EXPECT_TRUE(server.WaitForCommand({0x02, DAB_SAMPLE_RATE}));
  EXPECT_TRUE(server.WaitForCommand({0x03, 1}));
  EXPECT_TRUE(server.WaitForCommand({0x08, 0}));
  EXPECT_TRUE(server.WaitForCommand({0x05, static_cast<uint32_t>(-3)}));

  EXPECT_TRUE(source.SetFrequency(178352000));
  EXPECT_TRUE(server.WaitForCommand({0x01, 178352000}));

  EXPECT_TRUE(source.SetGain(297));
  EXPECT_TRUE(server.WaitForCommand({0x04, 297}));
}

TEST(RtlTcpSource, OpenFailsForOtherServers)
{
  CFakeRtlTcpServer server("HTTP");
  ASSERT_NE(server.GetPort(), 0);

  CRtlTcpSource source("127.0.0.1", server.GetPort(), 0);
  EXPECT_FALSE(source.Open());
}

TEST(RtlTcpSource, OpenFailsWithoutServer)
{
  CRtlTcpSource source("127.0.0.1", 1, 0);
  EXPECT_FALSE(source.Open());
}

TEST(RtlTcpSource, AvailableWithServer)
{
  CFakeRtlTcpServer server;
  ASSERT_NE(server.GetPort(), 0);

  EXPECT_TRUE(CRtlTcpSource("127.0.0.1", server.GetPort(), 0).IsAvailable());
}

TEST(RtlTcpSource, NotAvailableWithoutServer)
{
  EXPECT_FALSE(CRtlTcpSource("127.0.0.1", 1, 0).IsAvailable());
}

TEST(RtlTcpSource, NotAvailableWhileServerIsBusy)
{
  CFakeRtlTcpServer server("");
  ASSERT_NE(server.GetPort(), 0);

  CRtlTcpSource source("127.0.0.1", server.GetPort(), 0);
  EXPECT_FALSE(source.IsAvailable());
}

TEST(RtlTcpSource, StreamsSamples)
{
  CFakeRtlTcpServer server;
  ASSERT_NE(server.GetPort(), 0);

  CRtlTcpSource source("127.0.0.1", server.GetPort(), 0);
  ASSERT_TRUE(source.Open());

  std::mutex mutex;
  std::condition_variable condition;
  std::vector<uint8_t> received;
  bool allEven{true};
  ASSERT_TRUE(source.Start(
      [&](std::span<const uint8_t> samples)
      {
        std::lock_guard<std::mutex> lock(mutex);
        allEven = allEven && samples.size() % 2 == 0;
        received.insert(received.end(), samples.begin(), samples.end());
        condition.notify_all();
      }));

  std::vector<uint8_t> sent(3 * 65536);
  for (size_t i = 0; i < sent.size(); ++i)
    sent[i] = static_cast<uint8_t>(i * 7);
  server.Send(sent);

  {
    std::unique_lock<std::mutex> lock(mutex);
    EXPECT_TRUE(condition.wait_for(lock, 2s, [&] { return received.size() >= sent.size(); }));
  }
  source.Stop();

  EXPECT_TRUE(allEven);
  EXPECT_EQ(received, sent);
}

TEST(RtlTcpSource, TunerGains)
{
  EXPECT_EQ(CRtlTcpSource::GetTunerGains(TUNER_R820T).size(), 29u);
  EXPECT_EQ(CRtlTcpSource::GetTunerGains(6), CRtlTcpSource::GetTunerGains(TUNER_R820T));
  EXPECT_TRUE(CRtlTcpSource::GetTunerGains(0).empty());
}
