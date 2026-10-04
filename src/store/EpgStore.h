/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace DABPLUS
{

struct EpgEvent
{
  //! Unique per channel
  uint32_t id{0};
  std::time_t start{0};
  std::time_t end{0};
  std::string title;
  std::string plotOutline;
  std::string plot;
  std::string genre;

  bool operator==(const EpgEvent& other) const = default;
};

struct EpgChanges
{
  std::vector<EpgEvent> created;
  std::vector<EpgEvent> updated;
  std::vector<EpgEvent> deleted;

  bool IsEmpty() const { return created.empty() && updated.empty() && deleted.empty(); }
};

/*!
 * \brief The programme guide of all channels as received.
 */
class CEpgStore
{
public:
  /*!
   * \brief Replaces the events of a channel that start within the scope by the given events.
   * \param scopeStart, scopeEnd the scope of the schedule; both 0 for the time span of the events
   */
  EpgChanges ApplySchedule(int uid,
                           std::time_t scopeStart,
                           std::time_t scopeEnd,
                           const std::vector<EpgEvent>& events);

  /*!
   * \brief The events of a channel overlapping the given time span, ordered by start time.
   */
  std::vector<EpgEvent> GetEvents(int uid, std::time_t start, std::time_t end) const;

  /*!
   * \brief Removes all events that ended before the given time.
   */
  void RemoveEndedBefore(std::time_t time);

  std::string ToJson() const;

  /*!
   * \return false if the data could not be parsed; the store is unchanged then
   */
  bool FromJson(std::string_view json);

private:
  //! By channel uid and event id
  std::map<int, std::map<uint32_t, EpgEvent>> m_events;
};

} // namespace DABPLUS
