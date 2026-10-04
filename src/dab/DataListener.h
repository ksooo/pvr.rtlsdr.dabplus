/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace DABPLUS
{

/*!
 * \brief A complete MOT object (ETSI EN 301 234).
 */
struct MotObject
{
  std::string contentName;
  uint8_t contentType{0};
  uint16_t contentSubType{0};
  //! User application specific header parameters, by parameter id
  std::map<uint8_t, std::vector<uint8_t>> parameters;
  std::vector<uint8_t> body;
};

/*!
 * \brief Receives the MOT objects of the SPI data services of the tuned ensemble. Called on the
 * decoder threads, possibly concurrently.
 */
class IDataListener
{
public:
  virtual ~IDataListener() = default;

  virtual void OnMotObject(const MotObject& object) = 0;
};

} // namespace DABPLUS
