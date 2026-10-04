/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace DABPLUS
{

class CSpiCollector;
class ISpiTuner;

struct SpiUpdateTimeouts
{
  //! Until the data services of the ensemble are known
  std::chrono::milliseconds ensemble{15000};
  //! Without any new object once everything referenced has been received
  std::chrono::milliseconds quiet{90000};
  //! A carousel with the logos of dozens of services takes more than ten minutes
  std::chrono::milliseconds maximum{900000};
  std::chrono::milliseconds pollInterval{200};
};

enum class SpiUpdateResult
{
  DONE,
  INCOMPLETE,
  NO_SPI,
  NO_ENSEMBLE,
  ABORTED
};

/*!
 * \brief Receives the SPI data of one ensemble after another.
 */
class CSpiUpdater
{
public:
  using AbortCallback = std::function<bool()>;

  explicit CSpiUpdater(SpiUpdateTimeouts timeouts = {}) : m_timeouts(timeouts) {}

  /*!
   * \brief Tunes to the ensemble and waits until its SPI carousel has been received completely.
   *
   * The collector must receive the SPI data of the tuner.
   */
  SpiUpdateResult UpdateEnsemble(ISpiTuner& tuner,
                                 uint32_t frequency,
                                 CSpiCollector& collector,
                                 const AbortCallback& isAborted) const;

private:
  const SpiUpdateTimeouts m_timeouts;
};

} // namespace DABPLUS
