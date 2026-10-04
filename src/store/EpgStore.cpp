/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "EpgStore.h"

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

EpgChanges CEpgStore::ApplySchedule(int uid,
                                    std::time_t scopeStart,
                                    std::time_t scopeEnd,
                                    const std::vector<EpgEvent>& events)
{
  EpgChanges changes;
  if (scopeStart == 0 && scopeEnd == 0)
  {
    if (events.empty())
      return changes;

    scopeStart = std::ranges::min(events, {}, &EpgEvent::start).start;
    scopeEnd = std::ranges::max(events, {}, &EpgEvent::end).end;
  }

  auto& channelEvents = m_events[uid];
  std::set<uint32_t> ids;
  for (const auto& event : events)
  {
    ids.insert(event.id);
    const auto [it, isNew] = channelEvents.try_emplace(event.id, event);
    if (isNew)
    {
      changes.created.emplace_back(event);
    }
    else if (it->second != event)
    {
      it->second = event;
      changes.updated.emplace_back(event);
    }
  }

  for (auto it = channelEvents.begin(); it != channelEvents.end();)
  {
    const EpgEvent& event = it->second;
    if (event.start >= scopeStart && event.start < scopeEnd && !ids.contains(event.id))
    {
      changes.deleted.emplace_back(event);
      it = channelEvents.erase(it);
    }
    else
    {
      ++it;
    }
  }
  return changes;
}

std::vector<EpgEvent> CEpgStore::GetEvents(int uid, std::time_t start, std::time_t end) const
{
  std::vector<EpgEvent> events;
  const auto channelEvents = m_events.find(uid);
  if (channelEvents == m_events.end())
    return events;

  for (const auto& [_, event] : channelEvents->second)
  {
    if (event.end > start && event.start < end)
      events.emplace_back(event);
  }
  std::ranges::sort(events, {}, &EpgEvent::start);
  return events;
}

void CEpgStore::RemoveEndedBefore(std::time_t time)
{
  for (auto& [_, channelEvents] : m_events)
    std::erase_if(channelEvents, [time](const auto& entry) { return entry.second.end < time; });
  std::erase_if(m_events, [](const auto& entry) { return entry.second.empty(); });
}

std::string CEpgStore::ToJson() const
{
  nlohmann::json channels = nlohmann::json::object();
  for (const auto& [uid, channelEvents] : m_events)
  {
    nlohmann::json events = nlohmann::json::array();
    for (const auto& [_, event] : channelEvents)
    {
      events.push_back({{"id", event.id},
                        {"start", event.start},
                        {"end", event.end},
                        {"title", event.title},
                        {"plotOutline", event.plotOutline},
                        {"plot", event.plot},
                        {"genre", event.genre}});
    }
    channels[std::to_string(uid)] = std::move(events);
  }

  const nlohmann::json root{{"version", FORMAT_VERSION}, {"channels", std::move(channels)}};
  return root.dump();
}

bool CEpgStore::FromJson(std::string_view json)
{
  try
  {
    const auto root = nlohmann::json::parse(json);
    if (root.at("version").get<int>() != FORMAT_VERSION)
    {
      Log(LogLevel::LEVEL_ERROR, "Unsupported programme guide version {}",
          root.at("version").get<int>());
      return false;
    }

    std::map<int, std::map<uint32_t, EpgEvent>> channels;
    const auto storedChannels = root.at("channels");
    for (const auto& [uid, storedEvents] : storedChannels.items())
    {
      auto& channelEvents = channels[std::stoi(uid)];
      for (const auto& e : storedEvents)
      {
        EpgEvent event;
        event.id = e.at("id").get<uint32_t>();
        event.start = e.at("start").get<std::time_t>();
        event.end = e.at("end").get<std::time_t>();
        event.title = e.at("title").get<std::string>();
        event.plotOutline = e.at("plotOutline").get<std::string>();
        event.plot = e.at("plot").get<std::string>();
        event.genre = e.at("genre").get<std::string>();
        channelEvents[event.id] = std::move(event);
      }
    }

    m_events = std::move(channels);
    return true;
  }
  catch (const std::exception& e)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to parse programme guide: {}", e.what());
    return false;
  }
}

} // namespace DABPLUS
