/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiUpdater.h"

#include "dab/Tuner.h"
#include "spi/SpiCollector.h"

#include <algorithm>
#include <thread>

namespace DABPLUS
{

SpiUpdateResult CSpiUpdater::UpdateEnsemble(ISpiTuner& tuner,
                                            uint32_t frequency,
                                            CSpiCollector& collector,
                                            const AbortCallback& isAborted) const
{
  using Clock = std::chrono::steady_clock;

  if (isAborted())
    return SpiUpdateResult::ABORTED;

  collector.ResetStatus();
  if (!tuner.Tune(frequency))
    return isAborted() ? SpiUpdateResult::ABORTED : SpiUpdateResult::NO_ENSEMBLE;

  // The user applications of data services may be signalled only after the ensemble is complete
  const auto ensembleDeadline = Clock::now() + m_timeouts.ensemble;
  while (!tuner.HasSpiService())
  {
    if (isAborted())
      return SpiUpdateResult::ABORTED;

    if (Clock::now() >= ensembleDeadline)
      return tuner.GetEnsemble() ? SpiUpdateResult::NO_SPI : SpiUpdateResult::NO_ENSEMBLE;

    std::this_thread::sleep_for(m_timeouts.pollInterval);
  }

  const auto start = Clock::now();
  while (Clock::now() < start + m_timeouts.maximum)
  {
    if (isAborted())
      return SpiUpdateResult::ABORTED;

    const SpiStatus status = collector.GetStatus();
    const auto lastActivity = std::max(start, status.lastNewObject);
    if (status.hasServiceInformation && status.missingLogos == 0 &&
        Clock::now() >= lastActivity + m_timeouts.quiet)
      return SpiUpdateResult::DONE;

    std::this_thread::sleep_for(m_timeouts.pollInterval);
  }
  return SpiUpdateResult::INCOMPLETE;
}

} // namespace DABPLUS
