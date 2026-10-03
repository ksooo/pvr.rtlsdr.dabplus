/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/DynamicLabel.h"

#include <cstdint>
#include <span>

namespace DABPLUS
{

struct AudioFormat
{
  uint32_t sampleRate{0};
  uint8_t channels{0};

  bool operator==(const AudioFormat& other) const = default;
};

/*!
 * \brief Receives the decoded audio and programme labels of the selected service. Called on the
 * decoder threads.
 */
class IServiceListener
{
public:
  virtual ~IServiceListener() = default;

  /*!
   * \param samples interleaved signed 16 bit samples
   */
  virtual void OnAudio(const AudioFormat& format, std::span<const int16_t> samples) = 0;

  virtual void OnLabel(const ProgrammeLabel& label) = 0;
};

} // namespace DABPLUS
