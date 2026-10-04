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
  //! Modulation error ratio in dB; 0 until a frame was demodulated
  float mer{0.0f};
  //! Power at the antenna input in dB, relative as tuners are not calibrated
  float level{-100.0f};
  //! Reed-Solomon codewords of the selected DAB+ service that could not be corrected
  int uncorrectable{0};
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

class ISpiTuner : public ITuner
{
public:
  /*!
   * \brief Whether the ensemble on the current frequency carries an SPI data service, as far as
   * known yet.
   */
  virtual bool HasSpiService() const = 0;
};

} // namespace DABPLUS
