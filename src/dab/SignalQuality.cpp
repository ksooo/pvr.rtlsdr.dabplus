/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SignalQuality.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace DABPLUS
{

namespace
{

constexpr float MAX_MER = 50.0f;
constexpr float MIN_POWER = -100.0f;

} // unnamed namespace

float CalculateMer(std::span<const std::complex<float>> carriers)
{
  if (carriers.empty())
    return 0.0f;

  constexpr float QUARTER = std::numbers::pi_v<float> / 2;
  double errorPower{0.0};
  for (const auto& carrier : carriers)
  {
    // The phase relative to the nearest ideal point, which lies in the middle of a quadrant
    const float phase = std::arg(carrier) - QUARTER / 2;
    const float error = phase - QUARTER * std::round(phase / QUARTER);
    errorPower += error * error;
  }
  errorPower /= static_cast<double>(carriers.size());

  if (errorPower <= 0.0)
    return MAX_MER;

  return std::min(MAX_MER, static_cast<float>(-10.0 * std::log10(errorPower)));
}

float CalculatePowerDb(std::span<const std::complex<float>> samples)
{
  if (samples.empty())
    return MIN_POWER;

  double power{0.0};
  for (const auto& sample : samples)
    power += std::norm(sample);
  power /= static_cast<double>(samples.size());

  if (power <= 0.0)
    return MIN_POWER;

  return std::max(MIN_POWER, static_cast<float>(10.0 * std::log10(power)));
}

} // namespace DABPLUS
