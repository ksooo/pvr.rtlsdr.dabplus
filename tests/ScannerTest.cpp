/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/Tuner.h"
#include "scan/Scanner.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <map>

#include <gtest/gtest.h>

using namespace DABPLUS;
using namespace std::chrono_literals;

namespace
{

constexpr std::array BLOCKS{DabBlock{"5A", 1}, DabBlock{"5B", 2}, DabBlock{"5C", 3},
                            DabBlock{"5D", 4}, DabBlock{"6A", 5}};

constexpr ScanTimeouts TIMEOUTS{
    .sync = 30ms, .ensemble = 30ms, .services = 60ms, .settle = 10ms, .pollInterval = 1ms};

enum class Reception
{
  TUNE_FAILS,
  NOISE,
  SYNC_ONLY,
  INCOMPLETE_ENSEMBLE,
  COMPLETE_ENSEMBLE,
  // Receives the ensemble of the next lower frequency
  NEIGHBOUR_ENSEMBLE,
  // Complete from the start, but one more service appears every 5 ms until there are three
  GROWING_ENSEMBLE,
};

class CFakeTuner : public ITuner
{
public:
  explicit CFakeTuner(std::map<uint32_t, Reception> reception) : m_reception(std::move(reception))
  {
  }

  bool Tune(uint32_t frequency) override
  {
    m_frequency = frequency;
    m_tuneTime = std::chrono::steady_clock::now();
    ++m_tuneCount;
    return GetReception() != Reception::TUNE_FAILS;
  }

  TunerStatus GetStatus() const override
  {
    const Reception reception = GetReception();
    return {.isSynced = reception != Reception::NOISE && reception != Reception::TUNE_FAILS};
  }

  std::optional<EnsembleInfo> GetEnsemble() const override
  {
    const Reception reception = GetReception();
    if (reception == Reception::TUNE_FAILS || reception == Reception::NOISE ||
        reception == Reception::SYNC_ONLY)
      return {};

    const uint32_t origin =
        reception == Reception::NEIGHBOUR_ENSEMBLE ? m_frequency - 1 : m_frequency;
    EnsembleInfo ensemble{.eid = static_cast<uint16_t>(origin),
                          .label = "Ensemble " + std::to_string(origin),
                          .frequency = m_frequency};
    size_t serviceCount{1};
    if (reception == Reception::GROWING_ENSEMBLE)
      serviceCount = std::min<size_t>(3, 1 + (std::chrono::steady_clock::now() - m_tuneTime) / 5ms);
    for (size_t i = 0; i < serviceCount; ++i)
      ensemble.services.push_back(
          {.sid = static_cast<uint16_t>(origin * 10 + i), .label = "Service"});
    ensemble.isComplete = reception != Reception::INCOMPLETE_ENSEMBLE;
    return ensemble;
  }

  int m_tuneCount{0};

private:
  Reception GetReception() const
  {
    const auto it = m_reception.find(m_frequency);
    return it == m_reception.end() ? Reception::NOISE : it->second;
  }

  const std::map<uint32_t, Reception> m_reception;
  uint32_t m_frequency{0};
  std::chrono::steady_clock::time_point m_tuneTime;
};

} // unnamed namespace

TEST(Scanner, CollectsEnsemblesOfAllBlocks)
{
  CFakeTuner tuner({{1, Reception::TUNE_FAILS},
                    {2, Reception::SYNC_ONLY},
                    {3, Reception::COMPLETE_ENSEMBLE},
                    {5, Reception::INCOMPLETE_ENSEMBLE}});

  const auto ensembles = CScanner(TIMEOUTS).Run(tuner, BLOCKS, [](const auto&) { return true; });

  ASSERT_TRUE(ensembles);
  ASSERT_EQ(ensembles->size(), 2u);
  EXPECT_EQ((*ensembles)[0].frequency, 3u);
  EXPECT_EQ((*ensembles)[1].frequency, 5u);
  EXPECT_EQ(tuner.m_tuneCount, 5);
}

TEST(Scanner, SkipsEnsembleFoundOnNeighbourBlock)
{
  CFakeTuner tuner({{3, Reception::COMPLETE_ENSEMBLE}, {4, Reception::NEIGHBOUR_ENSEMBLE}});

  const auto ensembles = CScanner(TIMEOUTS).Run(tuner, BLOCKS, [](const auto&) { return true; });

  ASSERT_TRUE(ensembles);
  ASSERT_EQ(ensembles->size(), 1u);
  EXPECT_EQ((*ensembles)[0].frequency, 3u);
}

TEST(Scanner, WaitsForStableServiceList)
{
  CFakeTuner tuner({{2, Reception::GROWING_ENSEMBLE}});

  const auto ensembles = CScanner(TIMEOUTS).Run(tuner, BLOCKS, [](const auto&) { return true; });

  ASSERT_TRUE(ensembles);
  ASSERT_EQ(ensembles->size(), 1u);
  EXPECT_EQ((*ensembles)[0].services.size(), 3u);
}

TEST(Scanner, ReportsProgress)
{
  CFakeTuner tuner({{2, Reception::COMPLETE_ENSEMBLE}, {4, Reception::COMPLETE_ENSEMBLE}});

  std::vector<ScanProgress> reports;
  CScanner(TIMEOUTS).Run(tuner, BLOCKS,
                         [&reports](const ScanProgress& progress)
                         {
                           reports.push_back(progress);
                           return true;
                         });

  ASSERT_FALSE(reports.empty());
  EXPECT_EQ(reports.front().blockIndex, 0u);
  EXPECT_EQ(reports.front().blockCount, BLOCKS.size());
  EXPECT_EQ(reports.back().block.label, "6A");
  EXPECT_EQ(reports.back().ensemblesFound, 2u);
  EXPECT_EQ(reports.back().servicesFound, 2u);
}

TEST(Scanner, CancelReturnsNothing)
{
  CFakeTuner tuner({{1, Reception::COMPLETE_ENSEMBLE}});

  int calls{0};
  const auto ensembles =
      CScanner(TIMEOUTS).Run(tuner, BLOCKS, [&calls](const auto&) { return ++calls < 3; });

  EXPECT_FALSE(ensembles);
  EXPECT_LT(tuner.m_tuneCount, static_cast<int>(BLOCKS.size()));
}
