/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace DABPLUS
{

/*!
 * \brief Chooses the tuner gain from the level of the 8 bit samples.
 *
 * The level is the envelope of the sample magnitudes with fast attack and slow release. The gain
 * is lowered when the level gets close to the ADC's full scale, and raised when the next higher
 * gain step would still stay below that limit.
 */
class CSoftwareAgc
{
public:
  /*!
   * \param gains the tuner gains in 1/10 dB, in ascending order
   */
  explicit CSoftwareAgc(std::vector<int> gains);

  /*!
   * \brief Starts again from the middle of the gain range and forgets which gains overloaded,
   * e.g. after tuning.
   */
  void Reset();

  /*!
   * \brief Feeds a block of interleaved I/Q samples.
   * \return the new gain in 1/10 dB, if it has to change
   */
  std::optional<int> Process(std::span<const uint8_t> samples);

  int GetGain() const;

private:
  const std::vector<int> m_gains;
  size_t m_gainIndex{0};
  size_t m_maxGainIndex{0};
  float m_level{0.0f};
  unsigned int m_blockCount{0};
};

} // namespace DABPLUS
