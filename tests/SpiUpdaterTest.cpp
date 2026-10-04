/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiTestData.h"
#include "dab/Tuner.h"
#include "spi/SpiCollector.h"
#include "spi/SpiUpdater.h"

#include <atomic>
#include <functional>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace SPI_TEST;
using namespace std::chrono_literals;

namespace
{

constexpr uint32_t FREQUENCY = 178352000;

constexpr SpiUpdateTimeouts TIMEOUTS{
    .ensemble = 200ms, .quiet = 100ms, .maximum = 1000ms, .pollInterval = 5ms};

class CNullHandler : public ISpiHandler
{
public:
  void OnLogo(const std::vector<SpiServiceId>&, const std::vector<uint8_t>&) override {}
  void OnSchedule(const SpiSchedule&) override {}
};

class CFakeTuner : public ISpiTuner
{
public:
  bool Tune(uint32_t frequency) override
  {
    tunedFrequency = frequency;
    if (onTune)
      onTune();
    return canTune;
  }

  TunerStatus GetStatus() const override { return {}; }

  std::optional<EnsembleInfo> GetEnsemble() const override
  {
    if (!hasEnsemble)
      return {};
    return EnsembleInfo{.label = "DR Deutschland", .frequency = tunedFrequency};
  }

  bool HasSpiService() const override { return hasSpi; }

  bool canTune{true};
  bool hasEnsemble{true};
  bool hasSpi{true};
  uint32_t tunedFrequency{0};
  std::function<void()> onTune;
};

MotObject MakeServiceInformation()
{
  return {.contentName = "si.EIB",
          .contentType = 7,
          .contentSubType = 0,
          .body = ServiceInformation(
              {Service(Bearer(0xE0, 0x10BC, 0xD210), {Logo("d210.png", 128, 128)})})};
}

MotObject MakeLogo()
{
  return {.contentName = "d210.png", .contentType = 2, .contentSubType = 3, .body = {1, 2, 3}};
}

bool NeverAborted()
{
  return false;
}

} // unnamed namespace

TEST(SpiUpdater, DoneOnceEverythingWasReceived)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  CFakeTuner tuner;
  tuner.onTune = [&collector]
  {
    collector.OnMotObject(MakeLogo());
    collector.OnMotObject(MakeServiceInformation());
  };

  const auto start = std::chrono::steady_clock::now();
  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::DONE);
  EXPECT_GE(std::chrono::steady_clock::now() - start, TIMEOUTS.quiet);
  EXPECT_LT(std::chrono::steady_clock::now() - start, TIMEOUTS.maximum);
  EXPECT_EQ(tuner.tunedFrequency, FREQUENCY);
}

TEST(SpiUpdater, IncompleteWhileLogoIsMissing)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  CFakeTuner tuner;
  tuner.onTune = [&collector] { collector.OnMotObject(MakeServiceInformation()); };

  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::INCOMPLETE);
  EXPECT_EQ(collector.GetStatus().missingLogos, 1u);
}

TEST(SpiUpdater, ObjectsOfPreviousEnsembleDoNotCount)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  collector.OnMotObject(MakeLogo());
  collector.OnMotObject(MakeServiceInformation());
  CFakeTuner tuner;

  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::INCOMPLETE);
}

TEST(SpiUpdater, EnsembleWithoutSpi)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  CFakeTuner tuner;
  tuner.hasSpi = false;

  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::NO_SPI);
}

TEST(SpiUpdater, NoEnsemble)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  CFakeTuner tuner;
  tuner.hasSpi = false;
  tuner.hasEnsemble = false;
  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::NO_ENSEMBLE);

  tuner.canTune = false;
  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, NeverAborted),
            SpiUpdateResult::NO_ENSEMBLE);
}

TEST(SpiUpdater, Aborted)
{
  CNullHandler handler;
  CSpiCollector collector(handler);
  CFakeTuner tuner;
  std::atomic<int> polls{0};
  const auto isAborted = [&polls] { return ++polls > 10; };

  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, isAborted),
            SpiUpdateResult::ABORTED);
  EXPECT_EQ(CSpiUpdater(TIMEOUTS).UpdateEnsemble(tuner, FREQUENCY, collector, [] { return true; }),
            SpiUpdateResult::ABORTED);
}
