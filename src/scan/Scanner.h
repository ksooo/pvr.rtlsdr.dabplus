/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/BandIII.h"
#include "dab/ServiceInfo.h"

#include <chrono>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace DABPLUS
{

class ITuner;

struct ScanTimeouts
{
  std::chrono::milliseconds sync{5000};
  std::chrono::milliseconds ensemble{6000};
  std::chrono::milliseconds services{8000};
  std::chrono::milliseconds settle{1000};
  std::chrono::milliseconds pollInterval{100};
};

struct ScanProgress
{
  size_t blockIndex{0};
  size_t blockCount{0};
  DabBlock block;
  size_t ensemblesFound{0};
  size_t servicesFound{0};
};

class CScanner
{
public:
  /*!
   * \return false to cancel the scan
   */
  using ProgressCallback = std::function<bool(const ScanProgress& progress)>;

  explicit CScanner(ScanTimeouts timeouts = {}) : m_timeouts(timeouts) {}

  /*!
   * \brief Tunes to each block in turn and collects the ensembles found.
   *
   * Waits for the demodulator to synchronise, then for the ensemble label, then until the ensemble
   * is complete and its service list has been stable for the settle time. On timeout in the last
   * stage the services received so far are kept.
   *
   * \return the ensembles found, or nothing if the scan was cancelled
   */
  std::optional<std::vector<EnsembleInfo>> Run(ITuner& tuner,
                                               std::span<const DabBlock> blocks,
                                               const ProgressCallback& progress) const;

private:
  const ScanTimeouts m_timeouts;
};

} // namespace DABPLUS
