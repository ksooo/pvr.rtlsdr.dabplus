/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SoftwareAgc.h"

#include <cmath>
#include <cstdlib>
#include <utility>

namespace DABPLUS
{

namespace
{

constexpr float ATTACK = 0.1f;
constexpr float RELEASE = 0.00005f;

// Maximum sample magnitude is 128; leave headroom so that peaks do not clip
constexpr float LEVEL_MAX = 105.0f;

// A gain step must keep the level at least this far below LEVEL_MAX, in 1/10 dB
constexpr int STEP_UP_MARGIN = 5;

// Evaluate only every few blocks so that the level settles after a gain change
constexpr unsigned int BLOCKS_PER_DECISION = 4;

} // unnamed namespace

CSoftwareAgc::CSoftwareAgc(std::vector<int> gains) : m_gains(std::move(gains))
{
  Reset();
}

void CSoftwareAgc::Reset()
{
  m_gainIndex = m_gains.size() / 2;
  m_maxGainIndex = m_gains.empty() ? 0 : m_gains.size() - 1;
  m_level = 0.0f;
  m_blockCount = 0;
}

int CSoftwareAgc::GetGain() const
{
  return m_gains.empty() ? 0 : m_gains[m_gainIndex];
}

std::optional<int> CSoftwareAgc::Process(std::span<const uint8_t> samples)
{
  for (const uint8_t sample : samples)
  {
    const float magnitude = static_cast<float>(std::abs(static_cast<int>(sample) - 128));
    const float coefficient = magnitude > m_level ? ATTACK : RELEASE;
    m_level += coefficient * (magnitude - m_level);
  }

  if (m_gains.size() < 2 || ++m_blockCount < BLOCKS_PER_DECISION)
    return {};

  m_blockCount = 0;

  if (m_level > LEVEL_MAX && m_gainIndex > 0)
  {
    // The actual gain steps can be larger than the nominal ones; never return to a gain that
    // overloaded, otherwise the gain would oscillate between two steps
    m_maxGainIndex = m_gainIndex - 1;
    --m_gainIndex;
    return m_gains[m_gainIndex];
  }

  if (m_gainIndex < m_maxGainIndex)
  {
    const int step = m_gains[m_gainIndex + 1] - m_gains[m_gainIndex] + STEP_UP_MARGIN;
    const float levelAfterStep = m_level * std::pow(10.0f, static_cast<float>(step) / 200.0f);
    if (levelAfterStep < LEVEL_MAX)
    {
      ++m_gainIndex;
      return m_gains[m_gainIndex];
    }
  }

  return {};
}

} // namespace DABPLUS
