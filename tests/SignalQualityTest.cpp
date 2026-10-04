/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/SignalQuality.h"

#include <numbers>
#include <vector>

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

constexpr float PI = std::numbers::pi_v<float>;

// The DQPSK points at odd multiples of 45°, each phase shifted by the given error
std::vector<std::complex<float>> MakeCarriers(float phaseError, float amplitude = 1.0f)
{
  std::vector<std::complex<float>> carriers;
  for (int i = 0; i < 1000; ++i)
  {
    const float point = PI / 4 + (i % 4) * PI / 2;
    const float error = i % 2 == 0 ? phaseError : -phaseError;
    carriers.push_back(std::polar(amplitude * (1 + i % 3), point + error));
  }
  return carriers;
}

} // unnamed namespace

TEST(SignalQuality, MerOfIdealCarriersIsCapped)
{
  EXPECT_FLOAT_EQ(CalculateMer(MakeCarriers(0.0f)), 50.0f);
}

TEST(SignalQuality, MerFromPhaseError)
{
  // A phase error of 0.1 is an error power of 0.01
  EXPECT_NEAR(CalculateMer(MakeCarriers(0.1f)), 20.0f, 0.01f);
  EXPECT_NEAR(CalculateMer(MakeCarriers(0.1f, 0.001f)), 20.0f, 0.01f);
}

TEST(SignalQuality, MerOfNoise)
{
  // Uniformly distributed phase errors within ±45° have a power of (π/4)²/3
  std::vector<std::complex<float>> carriers;
  for (int i = 0; i < 10000; ++i)
    carriers.push_back(std::polar(1.0f, 2 * PI * static_cast<float>(i) / 10000));

  EXPECT_NEAR(CalculateMer(carriers), 6.84f, 0.05f);
  EXPECT_FLOAT_EQ(CalculateMer({}), 0.0f);
}

TEST(SignalQuality, Power)
{
  const std::vector<std::complex<float>> fullScale(100, std::polar(1.0f, 0.5f));
  const std::vector<std::complex<float>> tenth(100, std::polar(0.1f, 1.5f));
  const std::vector<std::complex<float>> silence(100, 0.0f);

  EXPECT_NEAR(CalculatePowerDb(fullScale), 0.0f, 0.001f);
  EXPECT_NEAR(CalculatePowerDb(tenth), -20.0f, 0.001f);
  EXPECT_FLOAT_EQ(CalculatePowerDb(silence), -100.0f);
  EXPECT_FLOAT_EQ(CalculatePowerDb({}), -100.0f);
}
