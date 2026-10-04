/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceInfo.h"

#include <cstdint>
#include <ctime>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace DABPLUS
{

/*!
 * \brief A service as referenced by an SPI DAB bearer.
 */
struct SpiServiceId
{
  //! 0 if the bearer does not contain the ensemble
  uint8_t ecc{0};
  uint16_t eid{0};
  uint32_t sid{0};
  bool isLongSid{false};
  uint8_t scids{0};

  bool Matches(const ServiceInfo& service) const;
  bool operator==(const SpiServiceId& other) const = default;
};

struct SpiLogo
{
  //! The content name of the MOT object carrying the image
  std::string contentName;
  int width{0};
  int height{0};
};

struct SpiService
{
  std::vector<SpiServiceId> ids;
  std::vector<SpiLogo> logos;
};

struct SpiProgramme
{
  //! Unique within the SPI data of the service
  uint32_t shortId{0};
  std::string title;
  std::string shortDescription;
  std::string longDescription;
  std::string genre;
  std::time_t start{0};
  int duration{0}; // seconds

  bool operator==(const SpiProgramme& other) const = default;
};

struct SpiSchedule
{
  //! Empty if the schedule does not name its services
  std::vector<SpiServiceId> services;
  //! 0 if the schedule does not define its scope
  std::time_t scopeStart{0};
  std::time_t scopeEnd{0};
  std::vector<SpiProgramme> programmes;
};

/*!
 * \brief Decodes a binary encoded DAB bearer (ETSI TS 102 371).
 */
std::optional<SpiServiceId> DecodeSpiBearer(std::span<const uint8_t> data);

/*!
 * \brief Decodes a binary encoded time point (ETSI TS 102 371) to UTC.
 */
std::optional<std::time_t> DecodeSpiTime(std::span<const uint8_t> data);

/*!
 * \brief Decodes a binary encoded service information document (ETSI TS 102 371).
 * \return nothing if the data is malformed
 */
std::optional<std::vector<SpiService>> DecodeServiceInformation(std::span<const uint8_t> data);

/*!
 * \brief Decodes a binary encoded programme information document (ETSI TS 102 371).
 * \return nothing if the data is malformed
 */
std::optional<SpiSchedule> DecodeProgrammeInformation(std::span<const uint8_t> data);

} // namespace DABPLUS
