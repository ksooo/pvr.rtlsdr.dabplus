/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiCollector.h"

#include "utils/Hash.h"
#include "utils/Log.h"

#include <algorithm>
#include <optional>

namespace DABPLUS
{

namespace
{

// ETSI TS 101 756, table 17
constexpr uint8_t MOT_CONTENT_TYPE_IMAGE = 2;
constexpr uint8_t MOT_CONTENT_TYPE_SPI = 7;

// ETSI TS 102 371
constexpr uint16_t SPI_SERVICE_INFORMATION = 0;
constexpr uint16_t SPI_PROGRAMME_INFORMATION = 1;
constexpr uint8_t PARAMETER_SCOPE_START = 0x25;
constexpr uint8_t PARAMETER_SCOPE_END = 0x26;
constexpr uint8_t PARAMETER_SCOPE_ID = 0x27;

// A carousel carries a few logos per service; this covers several large ensembles
constexpr size_t MAX_STORED_IMAGES = 200;

const SpiLogo* SelectLogo(const std::vector<SpiLogo>& logos)
{
  const auto best = std::ranges::max_element(logos, {}, [](const SpiLogo& logo)
                                             { return logo.width * logo.height; });
  return best != logos.end() ? &*best : nullptr;
}

void ApplyScopeParameters(const MotObject& object, SpiSchedule& schedule)
{
  const auto& parameters = object.parameters;
  if (schedule.scopeStart == 0)
  {
    const auto start = parameters.find(PARAMETER_SCOPE_START);
    const auto end = parameters.find(PARAMETER_SCOPE_END);
    if (start != parameters.end() && end != parameters.end())
    {
      const auto startTime = DecodeSpiTime(start->second);
      const auto endTime = DecodeSpiTime(end->second);
      if (startTime && endTime)
      {
        schedule.scopeStart = *startTime;
        schedule.scopeEnd = *endTime;
      }
    }
  }

  if (schedule.services.empty())
  {
    const auto id = parameters.find(PARAMETER_SCOPE_ID);
    const auto service = id != parameters.end() ? DecodeSpiBearer(id->second) : std::nullopt;
    if (service)
      schedule.services.emplace_back(*service);
  }
}

} // unnamed namespace

void CSpiCollector::OnMotObject(const MotObject& object)
{
  std::vector<Logo> logos;
  std::optional<SpiSchedule> schedule;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (object.contentType == MOT_CONTENT_TYPE_SPI &&
        object.contentSubType == SPI_SERVICE_INFORMATION)
    {
      if (!IsUnchanged(object))
        ProcessServiceInformation(object, logos);
    }
    else if (object.contentType == MOT_CONTENT_TYPE_SPI &&
             object.contentSubType == SPI_PROGRAMME_INFORMATION)
    {
      if (!IsUnchanged(object))
      {
        schedule = DecodeProgrammeInformation(object.body);
        if (schedule)
          ApplyScopeParameters(object, *schedule);
        else
          Log(LogLevel::LEVEL_WARNING, "Unable to decode programme information '{}'",
              object.contentName);
      }
    }
    else if (object.contentType == MOT_CONTENT_TYPE_IMAGE)
    {
      ProcessImage(object, logos);
    }
    UpdateStatus(object);
  }

  // The handler is called without holding the lock, it may take a while
  for (const auto& logo : logos)
    m_handler.OnLogo(logo.services, logo.image);
  if (schedule)
    m_handler.OnSchedule(*schedule);
}

void CSpiCollector::ResetStatus()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_receivedSinceReset.clear();
  m_referencedSinceReset.clear();
  m_hasServiceInformation = false;
  m_lastNewObject = {};
}

SpiStatus CSpiCollector::GetStatus() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  SpiStatus status;
  status.hasServiceInformation = m_hasServiceInformation;
  status.missingLogos =
      std::ranges::count_if(m_referencedSinceReset, [this](const std::string& logo)
                            { return !m_receivedSinceReset.contains(logo); });
  status.lastNewObject = m_lastNewObject;
  return status;
}

void CSpiCollector::UpdateStatus(const MotObject& object)
{
  if (object.contentType != MOT_CONTENT_TYPE_SPI && object.contentType != MOT_CONTENT_TYPE_IMAGE)
    return;

  if (m_receivedSinceReset.insert(object.contentName).second)
    m_lastNewObject = std::chrono::steady_clock::now();

  if (object.contentType == MOT_CONTENT_TYPE_SPI &&
      object.contentSubType == SPI_SERVICE_INFORMATION)
  {
    m_hasServiceInformation = true;
    const auto logos = m_referencedLogos.find(object.contentName);
    if (logos != m_referencedLogos.end())
      m_referencedSinceReset.insert(logos->second.begin(), logos->second.end());
  }
}

bool CSpiCollector::IsUnchanged(const MotObject& object)
{
  const uint64_t hash = HashData(object.body);
  const auto [it, isNew] = m_hashes.try_emplace(object.contentName, hash);
  if (!isNew && it->second == hash)
    return true;

  it->second = hash;
  return false;
}

void CSpiCollector::ProcessServiceInformation(const MotObject& object, std::vector<Logo>& logos)
{
  const auto services = DecodeServiceInformation(object.body);
  if (!services)
  {
    Log(LogLevel::LEVEL_WARNING, "Unable to decode service information '{}'", object.contentName);
    return;
  }

  std::map<std::string, std::vector<SpiServiceId>> logoServices;
  for (const auto& service : *services)
  {
    if (const SpiLogo* logo = SelectLogo(service.logos))
    {
      auto& ids = logoServices[logo->contentName];
      ids.insert(ids.end(), service.ids.begin(), service.ids.end());
    }
  }

  auto& referencedLogos = m_referencedLogos[object.contentName];
  referencedLogos.clear();
  for (auto& [contentName, ids] : logoServices)
  {
    referencedLogos.emplace_back(contentName);
    auto& knownIds = m_logoServices[contentName];
    if (knownIds == ids)
      continue;

    knownIds = std::move(ids);
    // Pass the image on again with its next repetition
    m_hashes.erase(contentName);
  }

  for (auto it = m_images.begin(); it != m_images.end();)
  {
    const auto knownServices = m_logoServices.find(it->first);
    if (knownServices == m_logoServices.end())
    {
      ++it;
      continue;
    }

    m_hashes[it->first] = HashData(it->second);
    logos.push_back({knownServices->second, std::move(it->second)});
    it = m_images.erase(it);
  }
}

void CSpiCollector::ProcessImage(const MotObject& object, std::vector<Logo>& logos)
{
  const auto services = m_logoServices.find(object.contentName);
  if (services == m_logoServices.end())
  {
    StoreImage(object);
    return;
  }

  if (!IsUnchanged(object))
    logos.push_back({services->second, object.body});
}

void CSpiCollector::StoreImage(const MotObject& object)
{
  std::erase_if(m_images,
                [&object](const auto& image) { return image.first == object.contentName; });
  m_images.emplace_back(object.contentName, object.body);
  if (m_images.size() > MAX_STORED_IMAGES)
    m_images.pop_front();
}

} // namespace DABPLUS
