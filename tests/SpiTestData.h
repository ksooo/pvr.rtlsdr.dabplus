/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <string_view>
#include <vector>

// Encodes binary SPI documents (ETSI TS 102 371) for tests
namespace SPI_TEST
{

using Bytes = std::vector<uint8_t>;

inline Bytes Concat(std::initializer_list<Bytes> parts)
{
  Bytes result;
  for (const auto& part : parts)
    result.insert(result.end(), part.begin(), part.end());
  return result;
}

inline Bytes Element(uint8_t tag, const Bytes& content)
{
  Bytes result{tag};
  const size_t size = content.size();
  if (size < 0xFE)
  {
    result.push_back(static_cast<uint8_t>(size));
  }
  else if (size <= 0xFFFF)
  {
    result.insert(result.end(),
                  {0xFE, static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)});
  }
  else
  {
    result.insert(result.end(), {0xFF, static_cast<uint8_t>(size >> 16),
                                 static_cast<uint8_t>(size >> 8), static_cast<uint8_t>(size)});
  }
  result.insert(result.end(), content.begin(), content.end());
  return result;
}

inline Bytes Text(std::string_view text)
{
  return {text.begin(), text.end()};
}

inline Bytes Cdata(std::string_view text)
{
  return Element(0x01, Text(text));
}

//! A time point with seconds (UTC flag set)
inline Bytes Time(std::time_t utc)
{
  const uint32_t mjd = static_cast<uint32_t>(utc / 86400 + 40587);
  const uint32_t secondOfDay = static_cast<uint32_t>(utc % 86400);
  const uint32_t value =
      (mjd << 14) | (1 << 11) | ((secondOfDay / 3600) << 6) | ((secondOfDay / 60) % 60);
  return {static_cast<uint8_t>(value >> 24),
          static_cast<uint8_t>(value >> 16),
          static_cast<uint8_t>(value >> 8),
          static_cast<uint8_t>(value),
          static_cast<uint8_t>((secondOfDay % 60) << 2),
          0};
}

inline Bytes Bearer(uint8_t ecc, uint16_t eid, uint16_t sid, uint8_t scids = 0)
{
  return {static_cast<uint8_t>(0x40 | scids), ecc,
          static_cast<uint8_t>(eid >> 8),     static_cast<uint8_t>(eid),
          static_cast<uint8_t>(sid >> 8),     static_cast<uint8_t>(sid)};
}

inline Bytes Uint16(uint16_t value)
{
  return {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
}

inline Bytes Uint24(uint32_t value)
{
  return {static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
          static_cast<uint8_t>(value)};
}

//! A logo of type logo_unrestricted
inline Bytes Logo(std::string_view contentName, uint16_t width, uint16_t height)
{
  return Element(
      0x13, Element(0x2B, Concat({Element(0x82, Text(contentName)), Element(0x83, {0x02}),
                                  Element(0x84, Uint16(width)), Element(0x85, Uint16(height))})));
}

inline Bytes Service(const Bytes& bearer, std::initializer_list<Bytes> logos)
{
  Bytes content = Element(0x29, Element(0x80, bearer));
  for (const auto& logo : logos)
    content = Concat({content, logo});
  return Element(0x28, content);
}

inline Bytes ServiceInformation(std::initializer_list<Bytes> services)
{
  Bytes ensemble = Element(0x80, {0xE0, 0x10, 0xBC});
  for (const auto& service : services)
    ensemble = Concat({ensemble, service});
  return Element(0x03, Element(0x26, ensemble));
}

inline Bytes Programme(uint32_t shortId,
                       std::string_view title,
                       std::time_t start,
                       uint16_t duration,
                       std::string_view description = {})
{
  Bytes content = Concat({Element(0x81, Uint24(shortId)), Element(0x11, Cdata(title)),
                          Element(0x19, Element(0x2C, Concat({Element(0x80, Time(start)),
                                                              Element(0x81, Uint16(duration))})))});
  if (!description.empty())
    content = Concat({content, Element(0x13, Element(0x1B, Cdata(description)))});
  return Element(0x1C, content);
}

inline Bytes Scope(std::time_t start, std::time_t end, const Bytes& bearer)
{
  return Element(0x24, Concat({Element(0x80, Time(start)), Element(0x81, Time(end)),
                               Element(0x25, Element(0x80, bearer))}));
}

inline Bytes ProgrammeInformation(const Bytes& scheduleContent)
{
  return Element(0x02, Element(0x21, scheduleContent));
}

} // namespace SPI_TEST
