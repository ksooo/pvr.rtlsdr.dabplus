/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "ChannelStore.h"

#include "dab/BandIII.h"
#include "utils/Log.h"

#include <algorithm>
#include <set>

#include <nlohmann/json.hpp>

namespace DABPLUS
{

namespace
{

constexpr int FORMAT_VERSION = 1;

} // unnamed namespace

std::vector<ServiceInfo> CChannelStore::GetServices() const
{
  std::vector<ServiceInfo> services;
  std::set<int> uids;
  for (const auto& ensemble : m_ensembles)
  {
    for (const auto& service : ensemble.services)
    {
      if (uids.insert(service.GetUid()).second)
        services.emplace_back(service);
    }
  }
  return services;
}

std::string CChannelStore::GetGroupName(const EnsembleInfo& ensemble) const
{
  const auto sameLabel = std::ranges::count(m_ensembles, ensemble.label, &EnsembleInfo::label);
  if (sameLabel < 2)
    return ensemble.label;

  const auto block = FindBlockByFrequency(ensemble.frequency);
  if (block)
    return fmt::format("{} ({})", ensemble.label, block->label);

  return fmt::format("{} ({:.3f} MHz)", ensemble.label, ensemble.frequency / 1e6);
}

std::string CChannelStore::ToJson() const
{
  nlohmann::json ensembles = nlohmann::json::array();
  for (const auto& ensemble : m_ensembles)
  {
    nlohmann::json services = nlohmann::json::array();
    for (const auto& service : ensemble.services)
    {
      services.push_back({{"ecc", service.ecc},
                          {"sid", service.sid},
                          {"scids", service.scids},
                          {"label", service.label},
                          {"shortLabel", service.shortLabel},
                          {"programmeType", service.programmeType},
                          {"dabPlus", service.isDabPlus}});
    }
    ensembles.push_back({{"ecc", ensemble.ecc},
                         {"eid", ensemble.eid},
                         {"label", ensemble.label},
                         {"shortLabel", ensemble.shortLabel},
                         {"frequency", ensemble.frequency},
                         {"services", std::move(services)}});
  }

  const nlohmann::json root{{"version", FORMAT_VERSION}, {"ensembles", std::move(ensembles)}};
  return root.dump(2);
}

bool CChannelStore::FromJson(std::string_view json)
{
  try
  {
    const auto root = nlohmann::json::parse(json);
    if (root.at("version").get<int>() != FORMAT_VERSION)
    {
      Log(LogLevel::LEVEL_ERROR, "Unsupported channel list version {}",
          root.at("version").get<int>());
      return false;
    }

    std::vector<EnsembleInfo> ensembles;
    for (const auto& e : root.at("ensembles"))
    {
      EnsembleInfo ensemble;
      ensemble.ecc = e.at("ecc").get<uint8_t>();
      ensemble.eid = e.at("eid").get<uint16_t>();
      ensemble.label = e.at("label").get<std::string>();
      ensemble.shortLabel = e.at("shortLabel").get<std::string>();
      ensemble.frequency = e.at("frequency").get<uint32_t>();
      for (const auto& s : e.at("services"))
      {
        ServiceInfo service;
        service.ecc = s.at("ecc").get<uint8_t>();
        service.sid = s.at("sid").get<uint16_t>();
        service.scids = s.at("scids").get<uint8_t>();
        service.label = s.at("label").get<std::string>();
        service.shortLabel = s.at("shortLabel").get<std::string>();
        service.programmeType = s.at("programmeType").get<uint8_t>();
        service.isDabPlus = s.at("dabPlus").get<bool>();
        ensemble.services.emplace_back(std::move(service));
      }
      ensembles.emplace_back(std::move(ensemble));
    }

    m_ensembles = std::move(ensembles);
    return true;
  }
  catch (const nlohmann::json::exception& e)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to parse channel list: {}", e.what());
    return false;
  }
}

} // namespace DABPLUS
