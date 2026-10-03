/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DABPLUS
{

/*!
 * \brief An audio programme service, identified by ECC, SId and SCIdS.
 */
struct ServiceInfo
{
  uint8_t ecc{0};
  uint16_t sid{0};
  uint8_t scids{0};
  std::string label;
  std::string shortLabel;
  uint8_t programmeType{0};
  bool isDabPlus{false};

  /*!
   * \brief Unique id of the service, stable across scans and ensembles. Always a positive int.
   */
  constexpr int GetUid() const { return (ecc << 20) | (sid << 4) | (scids & 0x0F); }

  bool operator==(const ServiceInfo& other) const = default;
};

struct EnsembleInfo
{
  uint8_t ecc{0};
  uint16_t eid{0};
  std::string label;
  std::string shortLabel;
  uint32_t frequency{0}; // Hz
  std::vector<ServiceInfo> services;

  /*!
   * \brief Whether the receiver has seen all information the ensemble signals. Not persisted.
   */
  bool isComplete{false};
};

} // namespace DABPLUS
