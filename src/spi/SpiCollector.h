/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/DataListener.h"
#include "spi/SpiDecoder.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace DABPLUS
{

class ISpiHandler
{
public:
  virtual ~ISpiHandler() = default;

  /*!
   * \param services the services the logo belongs to
   * \param image the image as received, usually PNG
   */
  virtual void OnLogo(const std::vector<SpiServiceId>& services,
                      const std::vector<uint8_t>& image) = 0;

  virtual void OnSchedule(const SpiSchedule& schedule) = 0;
};

/*!
 * \brief What has been received since the status was reset.
 */
struct SpiStatus
{
  bool hasServiceInformation{false};
  //! Logos referenced by the service information that have not been received
  size_t missingLogos{0};
  //! When an object was received for the first time
  std::chrono::steady_clock::time_point lastNewObject;
};

/*!
 * \brief Assembles logos and schedules from the MOT objects of SPI data services (ETSI TS 102 371).
 *
 * Objects are repeated in a carousel; unchanged objects are passed on only once. Logos are passed
 * on once both the image and the service information referencing it have been received.
 */
class CSpiCollector : public IDataListener
{
public:
  explicit CSpiCollector(ISpiHandler& handler) : m_handler(handler) {}

  void OnMotObject(const MotObject& object) override;

  void ResetStatus();
  SpiStatus GetStatus() const;

private:
  struct Logo
  {
    std::vector<SpiServiceId> services;
    std::vector<uint8_t> image;
  };

  bool IsUnchanged(const MotObject& object);
  void ProcessServiceInformation(const MotObject& object, std::vector<Logo>& logos);
  void ProcessImage(const MotObject& object, std::vector<Logo>& logos);
  void StoreImage(const MotObject& object);
  void UpdateStatus(const MotObject& object);

  ISpiHandler& m_handler;

  mutable std::mutex m_mutex;
  //! Hash of the last body of each object passed on, by content name
  std::map<std::string, uint64_t> m_hashes;
  //! The services of each logo the service information refers to, by content name
  std::map<std::string, std::vector<SpiServiceId>> m_logoServices;
  //! Images received before the service information referencing them, oldest first
  std::deque<std::pair<std::string, std::vector<uint8_t>>> m_images;
  //! The logos referenced by each service information, by content name
  std::map<std::string, std::vector<std::string>> m_referencedLogos;

  std::set<std::string> m_receivedSinceReset;
  std::set<std::string> m_referencedSinceReset;
  bool m_hasServiceInformation{false};
  std::chrono::steady_clock::time_point m_lastNewObject;
};

} // namespace DABPLUS
