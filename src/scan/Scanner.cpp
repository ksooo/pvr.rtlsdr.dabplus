/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Scanner.h"

#include "dab/Tuner.h"
#include "utils/Log.h"

#include <algorithm>
#include <thread>

namespace DABPLUS
{

std::optional<std::vector<EnsembleInfo>> CScanner::Run(ITuner& tuner,
                                                       std::span<const DabBlock> blocks,
                                                       const ProgressCallback& progress) const
{
  using Clock = std::chrono::steady_clock;

  std::vector<EnsembleInfo> ensembles;
  size_t servicesFound{0};

  for (size_t i = 0; i < blocks.size(); ++i)
  {
    const DabBlock& block = blocks[i];
    const ScanProgress state{i, blocks.size(), block, ensembles.size(), servicesFound};

    // Polls until the condition holds or the timeout expires; nothing if cancelled
    const auto waitFor = [&](std::chrono::milliseconds timeout,
                             const std::function<bool()>& condition) -> std::optional<bool>
    {
      const auto deadline = Clock::now() + timeout;
      while (true)
      {
        if (!progress(state))
          return {};
        if (condition())
          return true;
        if (Clock::now() >= deadline)
          return false;
        std::this_thread::sleep_for(m_timeouts.pollInterval);
      }
    };

    if (!progress(state))
      return {};

    if (!tuner.Tune(block.frequency))
      continue;

    const auto synced = waitFor(m_timeouts.sync, [&] { return tuner.GetStatus().isSynced; });
    if (!synced)
      return {};
    if (!*synced)
      continue;

    const auto hasEnsemble =
        waitFor(m_timeouts.ensemble, [&] { return tuner.GetEnsemble().has_value(); });
    if (!hasEnsemble)
      return {};
    if (!*hasEnsemble)
    {
      Log(LogLevel::LEVEL_DEBUG, "Block {}: synchronised, but no ensemble information",
          block.label);
      continue;
    }

    // Services may still be added after the ensemble looks complete, so additionally wait until
    // the service list has not changed for a while
    size_t serviceCount{0};
    auto lastChange = Clock::now();
    const auto complete =
        waitFor(m_timeouts.services,
                [&]
                {
                  const auto ensemble = tuner.GetEnsemble();
                  if (!ensemble)
                    return false;
                  if (ensemble->services.size() != serviceCount)
                  {
                    serviceCount = ensemble->services.size();
                    lastChange = Clock::now();
                  }
                  return ensemble->isComplete && Clock::now() - lastChange >= m_timeouts.settle;
                });
    if (!complete)
      return {};

    auto ensemble = tuner.GetEnsemble();
    if (!ensemble || ensemble->services.empty())
      continue;

    // The demodulator also locks onto an ensemble on an overlapping neighbour block (e.g. 10A
    // and 10N); keep it on the block it was found on first
    const auto sameEnsemble = [&ensemble](const EnsembleInfo& other)
    { return other.ecc == ensemble->ecc && other.eid == ensemble->eid; };
    if (std::ranges::any_of(ensembles, sameEnsemble))
    {
      Log(LogLevel::LEVEL_DEBUG, "Block {}: ensemble '{}' already found on another block",
          block.label, ensemble->label);
      continue;
    }

    Log(LogLevel::LEVEL_INFO, "Block {}: ensemble '{}' with {} services{}", block.label,
        ensemble->label, ensemble->services.size(), *complete ? "" : " (incomplete)");
    servicesFound += ensemble->services.size();
    ensembles.emplace_back(std::move(*ensemble));
  }

  return ensembles;
}

} // namespace DABPLUS
