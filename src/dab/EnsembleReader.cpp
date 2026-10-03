/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "EnsembleReader.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include <dab/database/dab_database.h>

namespace DABPLUS
{

namespace
{

// DAB labels are padded with spaces to 16 characters
std::string TrimLabel(std::string_view label)
{
  const auto end = label.find_last_not_of(' ');
  return std::string{end == std::string_view::npos ? std::string_view{} : label.substr(0, end + 1)};
}

std::string ToLower(std::string_view text)
{
  std::string result{text};
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

const ServiceComponent* FindPrimaryAudioComponent(const DAB_Database& database,
                                                  const Service& service)
{
  const ServiceComponent* primary{nullptr};
  for (const auto& component : database.service_components)
  {
    if (component.service_id == service.id &&
        component.transport_mode == TransportMode::STREAM_MODE_AUDIO &&
        (!primary || component.component_id < primary->component_id))
      primary = &component;
  }
  return primary;
}

} // unnamed namespace

std::optional<EnsembleInfo> ReadEnsemble(const DAB_Database& database, uint32_t frequency)
{
  if (database.ensemble.label.empty())
    return {};

  EnsembleInfo ensemble;
  ensemble.ecc = database.ensemble.extended_country_code;
  ensemble.eid = database.ensemble.id.value;
  ensemble.label = TrimLabel(database.ensemble.label);
  ensemble.shortLabel = TrimLabel(database.ensemble.short_label);
  ensemble.frequency = frequency;

  bool allServicesComplete{true};
  for (const auto& service : database.services)
  {
    // Programme services always have 16 bit service ids, 32 bit ids denote data services
    if (service.id.type != ServiceIdType::BITS16)
      continue;

    const ServiceComponent* component = FindPrimaryAudioComponent(database, service);
    if (!component)
      continue;

    if (!service.is_complete || !component->is_complete || service.label.empty())
    {
      allServicesComplete = false;
      continue;
    }

    ServiceInfo info;
    info.ecc = ensemble.ecc;
    info.sid = static_cast<uint16_t>(service.id.get_unique_identifier());
    info.scids = component->component_id;
    info.label = TrimLabel(service.label);
    info.shortLabel = TrimLabel(service.short_label);
    info.programmeType = service.programme_type;
    info.isDabPlus = component->audio_service_type == AudioServiceType::DAB_PLUS;
    ensemble.services.emplace_back(std::move(info));
  }

  std::ranges::stable_sort(ensemble.services, {},
                           [](const ServiceInfo& service) { return ToLower(service.label); });

  // Without the ECC the service ids would not be stable, so wait for it as well. The number of
  // services (FIG 0/7) is optional.
  const size_t signalledServices = database.ensemble.nb_services;
  ensemble.isComplete = database.ensemble.is_complete && ensemble.ecc != 0 && allServicesComplete &&
                        !ensemble.services.empty() && database.services.size() >= signalledServices;
  return ensemble;
}

} // namespace DABPLUS
