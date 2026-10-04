/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/Receiver.h"
#include "device/IqFileSource.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace std::chrono_literals;

namespace
{

/*!
 * \brief Delivers random samples as fast as they are consumed and records the gains set.
 */
class CNoiseSource : public ISampleSource
{
public:
  bool IsAvailable() const override { return true; }
  bool Open() override { return true; }
  void Close() override {}

  bool Start(SamplesCallback callback) override
  {
    m_stop = false;
    m_thread = std::thread(
        [this, callback = std::move(callback)]
        {
          std::mt19937 generator;
          std::vector<uint8_t> block(65536);
          while (!m_stop)
          {
            for (auto& sample : block)
              sample = static_cast<uint8_t>(generator());
            callback(block);
            std::this_thread::sleep_for(16ms);
          }
        });
    return true;
  }

  void Stop() override
  {
    m_stop = true;
    if (m_thread.joinable())
      m_thread.join();
  }

  bool SetFrequency(uint32_t frequency) override
  {
    m_frequency = frequency;
    return true;
  }

  std::vector<int> GetGains() const override { return {0, 100, 200, 300, 400}; }

  bool SetGain(int gain) override
  {
    m_gain = gain;
    return true;
  }

  std::string GetName() const override { return "noise"; }

  std::atomic<uint32_t> m_frequency{0};
  std::atomic<int> m_gain{-1};

private:
  std::atomic<bool> m_stop{false};
  std::thread m_thread;
};

} // unnamed namespace

TEST(Receiver, TuneRequiresStart)
{
  CReceiver receiver(std::make_unique<CNoiseSource>(), {});
  EXPECT_FALSE(receiver.Tune(178352000));
}

TEST(Receiver, NoEnsembleInNoise)
{
  auto source = std::make_unique<CNoiseSource>();
  CNoiseSource& noise = *source;
  CReceiver receiver(std::move(source), {});

  ASSERT_TRUE(receiver.Start());
  EXPECT_EQ(noise.m_gain, 200);

  ASSERT_TRUE(receiver.Tune(178352000));
  EXPECT_EQ(noise.m_frequency, 178352000u);

  std::this_thread::sleep_for(500ms);
  EXPECT_FALSE(receiver.GetEnsemble());

  // Retuning while samples are flowing must be safe
  ASSERT_TRUE(receiver.Tune(180064000));
  std::this_thread::sleep_for(100ms);
  receiver.Stop();
}

TEST(Receiver, ManualGainUsesNearestStep)
{
  auto source = std::make_unique<CNoiseSource>();
  CNoiseSource& noise = *source;
  CReceiver receiver(std::move(source), {.automatic = false, .manualGain = 290});

  ASSERT_TRUE(receiver.Start());
  EXPECT_EQ(noise.m_gain, 300);
  receiver.Stop();
}

// Set DABPLUS_TEST_IQ_FILE to a raw rtl_sdr recording of an ensemble at 2.048 MS/s
TEST(Receiver, ReceivesEnsembleFromRecording)
{
  const char* path = std::getenv("DABPLUS_TEST_IQ_FILE");
  if (!path)
    GTEST_SKIP() << "DABPLUS_TEST_IQ_FILE not set";

  CReceiver receiver(std::make_unique<CIqFileSource>(path), {});
  ASSERT_TRUE(receiver.Start());
  ASSERT_TRUE(receiver.Tune(178352000));

  std::optional<EnsembleInfo> ensemble;
  const auto deadline = std::chrono::steady_clock::now() + 20s;
  while (std::chrono::steady_clock::now() < deadline)
  {
    ensemble = receiver.GetEnsemble();
    if (ensemble && ensemble->isComplete)
      break;
    std::this_thread::sleep_for(100ms);
  }
  receiver.Stop();

  ASSERT_TRUE(ensemble);
  EXPECT_TRUE(ensemble->isComplete);
  EXPECT_FALSE(ensemble->label.empty());
  EXPECT_FALSE(ensemble->services.empty());
  EXPECT_NE(ensemble->ecc, 0);
}

namespace
{

class CRecordingListener : public IServiceListener
{
public:
  void OnAudio(const AudioFormat& format, std::span<const int16_t> samples) override
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_format = format;
    m_samples += samples.size();
  }

  void OnLabel(const ProgrammeLabel& label) override
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_labels.push_back(label);
  }

  void OnPicture(std::string_view, std::span<const uint8_t>) override {}

  std::mutex m_mutex;
  AudioFormat m_format;
  size_t m_samples{0};
  std::vector<ProgrammeLabel> m_labels;
};

} // unnamed namespace

// Set DABPLUS_TEST_IQ_FILE to a recording of block 5C in Germany (Deutschlandfunk, SId D210)
TEST(Receiver, DecodesServiceFromRecording)
{
  const char* path = std::getenv("DABPLUS_TEST_IQ_FILE");
  if (!path)
    GTEST_SKIP() << "DABPLUS_TEST_IQ_FILE not set";

  CRecordingListener listener;
  CReceiver receiver(std::make_unique<CIqFileSource>(path), {});
  ASSERT_TRUE(receiver.Start());
  ASSERT_TRUE(receiver.Tune(178352000));
  receiver.SelectService(0xD210, 0, &listener);

  const auto deadline = std::chrono::steady_clock::now() + 20s;
  while (std::chrono::steady_clock::now() < deadline)
  {
    {
      std::lock_guard<std::mutex> lock(listener.m_mutex);
      if (listener.m_samples > 48000 * 2 * 3 && !listener.m_labels.empty())
        break;
    }
    std::this_thread::sleep_for(100ms);
  }
  const TunerStatus status = receiver.GetStatus();
  receiver.ClearService();
  receiver.Stop();

  std::lock_guard<std::mutex> lock(listener.m_mutex);
  EXPECT_EQ(listener.m_format, (AudioFormat{48000, 2}));
  EXPECT_GT(listener.m_samples, 48000u * 2 * 3);
  ASSERT_FALSE(listener.m_labels.empty());
  EXPECT_FALSE(listener.m_labels.back().text.empty());
  // The signal of the recording is decoded without errors
  EXPECT_GT(status.mer, 6.0f);
  EXPECT_EQ(status.uncorrectable, 0);
}
