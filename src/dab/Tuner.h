/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceInfo.h"

#include <cstdint>
#include <optional>

namespace DABPLUS
{

struct TunerStatus
{
  bool isSynced{false};
  int framesRead{0};
  int framesDesynced{0};
  int gain{0}; // 1/10 dB
};

class ITuner
{
public:
  virtual ~ITuner() = default;

  /*!
   * \brief Tunes to the given frequency and discards all state of the previous ensemble.
   */
  virtual bool Tune(uint32_t frequency) = 0;

  virtual TunerStatus GetStatus() const = 0;

  /*!
   * \brief The ensemble on the current frequency, once its label has been received.
   */
  virtual std::optional<EnsembleInfo> GetEnsemble() const = 0;
};

} // namespace DABPLUS
