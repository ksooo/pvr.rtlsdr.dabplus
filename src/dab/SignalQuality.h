/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <complex>
#include <span>

namespace DABPLUS
{

/*!
 * \brief The modulation error ratio of differentially demodulated QPSK carriers in dB, from the
 * deviation of their phases from the ideal points at odd multiples of 45°.
 *
 * Amplitude errors are not taken into account, as the amplitude of the carriers is not normalised.
 * \return 0 for no carriers; capped at 50 dB
 */
float CalculateMer(std::span<const std::complex<float>> carriers);

/*!
 * \brief The mean power of the samples in dB relative to samples of magnitude 1.
 * \return -100 for no or zero samples
 */
float CalculatePowerDb(std::span<const std::complex<float>> samples);

} // namespace DABPLUS
