/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "SpiDecoder.h"

#include <algorithm>
#include <array>
#include <map>

namespace DABPLUS
{

namespace
{

namespace TAG
{
constexpr uint8_t CDATA = 0x01;
constexpr uint8_t EPG = 0x02;
constexpr uint8_t SERVICE_INFORMATION = 0x03;
constexpr uint8_t TOKEN_TABLE = 0x04;
constexpr uint8_t DEFAULT_CONTENT_ID = 0x05;
constexpr uint8_t DEFAULT_LANGUAGE = 0x06;
constexpr uint8_t SHORT_NAME = 0x10;
constexpr uint8_t MEDIUM_NAME = 0x11;
constexpr uint8_t LONG_NAME = 0x12;
constexpr uint8_t MEDIA_DESCRIPTION = 0x13;
constexpr uint8_t GENRE = 0x14;
constexpr uint8_t LOCATION = 0x19;
constexpr uint8_t SHORT_DESCRIPTION = 0x1A;
constexpr uint8_t LONG_DESCRIPTION = 0x1B;
constexpr uint8_t PROGRAMME = 0x1C;
constexpr uint8_t SCHEDULE = 0x21;
constexpr uint8_t SCOPE = 0x24;
constexpr uint8_t SERVICE_SCOPE = 0x25;
constexpr uint8_t SERVICE = 0x28;
constexpr uint8_t SERVICE_ID = 0x29;
constexpr uint8_t MULTIMEDIA = 0x2B;
constexpr uint8_t TIME = 0x2C;
constexpr uint8_t BEARER = 0x2D;
} // namespace TAG

// Attribute tags are only unique within their element
namespace ATTRIBUTE
{
constexpr uint8_t ID = 0x80;
constexpr uint8_t PROGRAMME_SHORT_ID = 0x81;
constexpr uint8_t SCOPE_START = 0x80;
constexpr uint8_t SCOPE_STOP = 0x81;
constexpr uint8_t TIME_TIME = 0x80;
constexpr uint8_t TIME_DURATION = 0x81;
constexpr uint8_t MULTIMEDIA_URL = 0x82;
constexpr uint8_t MULTIMEDIA_TYPE = 0x83;
constexpr uint8_t MULTIMEDIA_WIDTH = 0x84;
constexpr uint8_t MULTIMEDIA_HEIGHT = 0x85;
} // namespace ATTRIBUTE

constexpr uint8_t LOGO_UNRESTRICTED = 0x02;
constexpr uint8_t LOGO_COLOUR_SQUARE = 0x04;
constexpr uint8_t LOGO_COLOUR_RECTANGLE = 0x06;

constexpr int MAX_DEPTH = 16;

// The Modified Julian Date of the Unix epoch
constexpr std::time_t MJD_UNIX_EPOCH = 40587;

struct Element
{
  uint8_t tag{0};
  //! Content of elements that are not made of further elements, e.g. CDATA
  std::span<const uint8_t> data;
  std::map<uint8_t, std::span<const uint8_t>> attributes;
  std::vector<Element> children;

  const Element* FindChild(uint8_t childTag) const
  {
    const auto it = std::ranges::find(children, childTag, &Element::tag);
    return it != children.end() ? &*it : nullptr;
  }

  std::optional<std::span<const uint8_t>> GetAttribute(uint8_t attributeTag) const
  {
    const auto it = attributes.find(attributeTag);
    if (it == attributes.end())
      return {};
    return it->second;
  }
};

uint32_t ReadUint(std::span<const uint8_t> data)
{
  uint32_t value{0};
  for (const uint8_t byte : data.first(std::min<size_t>(data.size(), 4)))
    value = (value << 8) | byte;
  return value;
}

bool HasRawContent(uint8_t tag)
{
  return tag == TAG::CDATA || tag == TAG::TOKEN_TABLE || tag == TAG::DEFAULT_CONTENT_ID ||
         tag == TAG::DEFAULT_LANGUAGE;
}

bool ParseContent(std::span<const uint8_t> content, Element& parent, int depth)
{
  if (depth > MAX_DEPTH)
    return false;

  while (!content.empty())
  {
    if (content.size() < 2)
      return false;

    const uint8_t tag = content[0];
    size_t length = content[1];
    size_t headerSize = 2;
    if (length == 0xFE)
    {
      headerSize = 4;
      if (content.size() < headerSize)
        return false;
      length = ReadUint(content.subspan(2, 2));
    }
    else if (length == 0xFF)
    {
      headerSize = 5;
      if (content.size() < headerSize)
        return false;
      length = ReadUint(content.subspan(2, 3));
    }
    if (content.size() < headerSize + length)
      return false;

    const auto data = content.subspan(headerSize, length);
    content = content.subspan(headerSize + length);

    if (tag >= 0x80)
    {
      parent.attributes[tag] = data;
      continue;
    }

    Element& element = parent.children.emplace_back();
    element.tag = tag;
    if (HasRawContent(tag))
      element.data = data;
    else if (!ParseContent(data, element, depth + 1))
      return false;
  }
  return true;
}

std::optional<Element> Parse(std::span<const uint8_t> data, uint8_t rootTag)
{
  Element document;
  if (!ParseContent(data, document, 0))
    return {};

  const Element* root = document.FindChild(rootTag);
  if (!root)
    return {};

  return *root;
}

class CStringDecoder
{
public:
  explicit CStringDecoder(const Element& root)
  {
    const Element* table = root.FindChild(TAG::TOKEN_TABLE);
    if (!table)
      return;

    auto data = table->data;
    while (data.size() >= 2)
    {
      const uint8_t id = data[0];
      const size_t length = std::min<size_t>(data[1], data.size() - 2);
      if (id < m_tokens.size())
        m_tokens[id].assign(data.begin() + 2, data.begin() + 2 + length);
      data = data.subspan(2 + length);
    }
  }

  std::string Decode(std::span<const uint8_t> data) const
  {
    std::string text;
    for (const uint8_t byte : data)
    {
      if (byte < m_tokens.size() && !m_tokens[byte].empty())
        text += m_tokens[byte];
      else
        text += static_cast<char>(byte);
    }
    return text;
  }

  //! The text of the CDATA of the first child element with the given tag
  std::string GetText(const Element& element, uint8_t tag) const
  {
    const Element* child = element.FindChild(tag);
    if (!child)
      return {};

    const Element* cdata = child->FindChild(TAG::CDATA);
    return cdata ? Decode(cdata->data) : std::string{};
  }

private:
  // Tokens are the control characters 0x01 to 0x13
  std::array<std::string, 0x14> m_tokens;
};

std::vector<SpiServiceId> GetServiceIds(const Element& service)
{
  std::vector<SpiServiceId> ids;
  for (const auto& child : service.children)
  {
    if (child.tag != TAG::SERVICE_ID && child.tag != TAG::BEARER)
      continue;

    const auto id = child.GetAttribute(ATTRIBUTE::ID);
    if (!id)
      continue;

    if (const auto serviceId = DecodeSpiBearer(*id))
      ids.emplace_back(*serviceId);
  }
  return ids;
}

std::optional<SpiLogo> GetLogo(const Element& multimedia, const CStringDecoder& strings)
{
  const auto url = multimedia.GetAttribute(ATTRIBUTE::MULTIMEDIA_URL);
  const auto type = multimedia.GetAttribute(ATTRIBUTE::MULTIMEDIA_TYPE);
  if (!url || !type || type->empty())
    return {};

  SpiLogo logo;
  logo.contentName = strings.Decode(*url);
  switch ((*type)[0])
  {
    case LOGO_COLOUR_SQUARE:
      logo.width = 32;
      logo.height = 32;
      break;
    case LOGO_COLOUR_RECTANGLE:
      logo.width = 112;
      logo.height = 32;
      break;
    case LOGO_UNRESTRICTED:
    {
      const auto width = multimedia.GetAttribute(ATTRIBUTE::MULTIMEDIA_WIDTH);
      const auto height = multimedia.GetAttribute(ATTRIBUTE::MULTIMEDIA_HEIGHT);
      if (!width || !height)
        return {};
      logo.width = static_cast<int>(ReadUint(*width));
      logo.height = static_cast<int>(ReadUint(*height));
      break;
    }
    default:
      return {};
  }

  // Logos may also be referenced by an URL to be fetched via IP
  if (logo.contentName.empty() || logo.contentName.find("://") != std::string::npos)
    return {};

  return logo;
}

void CollectServices(const Element& element,
                     const CStringDecoder& strings,
                     std::vector<SpiService>& services)
{
  for (const auto& child : element.children)
  {
    if (child.tag != TAG::SERVICE)
    {
      CollectServices(child, strings, services);
      continue;
    }

    SpiService service;
    service.ids = GetServiceIds(child);
    for (const auto& description : child.children)
    {
      if (description.tag != TAG::MEDIA_DESCRIPTION)
        continue;

      const Element* multimedia = description.FindChild(TAG::MULTIMEDIA);
      if (!multimedia)
        continue;

      if (auto logo = GetLogo(*multimedia, strings))
        service.logos.emplace_back(std::move(*logo));
    }

    if (!service.ids.empty())
      services.emplace_back(std::move(service));
  }
}

std::string GetDescription(const Element& programme, uint8_t tag, const CStringDecoder& strings)
{
  for (const auto& description : programme.children)
  {
    if (description.tag != TAG::MEDIA_DESCRIPTION)
      continue;

    std::string text = strings.GetText(description, tag);
    if (!text.empty())
      return text;
  }
  return {};
}

std::optional<SpiProgramme> GetProgramme(const Element& element, const CStringDecoder& strings)
{
  const auto shortId = element.GetAttribute(ATTRIBUTE::PROGRAMME_SHORT_ID);
  const Element* location = element.FindChild(TAG::LOCATION);
  const Element* time = location ? location->FindChild(TAG::TIME) : nullptr;
  if (!shortId || !time)
    return {};

  const auto start = time->GetAttribute(ATTRIBUTE::TIME_TIME);
  const auto duration = time->GetAttribute(ATTRIBUTE::TIME_DURATION);
  const auto startTime = start ? DecodeSpiTime(*start) : std::nullopt;
  if (!startTime || !duration)
    return {};

  SpiProgramme programme;
  programme.shortId = ReadUint(*shortId);
  programme.start = *startTime;
  programme.duration = static_cast<int>(ReadUint(*duration));

  for (const uint8_t tag : {TAG::LONG_NAME, TAG::MEDIUM_NAME, TAG::SHORT_NAME})
  {
    programme.title = strings.GetText(element, tag);
    if (!programme.title.empty())
      break;
  }
  programme.shortDescription = GetDescription(element, TAG::SHORT_DESCRIPTION, strings);
  programme.longDescription = GetDescription(element, TAG::LONG_DESCRIPTION, strings);

  for (const auto& genre : element.children)
  {
    if (genre.tag != TAG::GENRE)
      continue;

    const Element* cdata = genre.FindChild(TAG::CDATA);
    if (cdata)
    {
      programme.genre = strings.Decode(cdata->data);
      break;
    }
  }

  return programme;
}

} // unnamed namespace

bool SpiServiceId::Matches(const ServiceInfo& service) const
{
  return !isLongSid && sid == service.sid && scids == service.scids &&
         (ecc == 0 || ecc == service.ecc);
}

std::optional<SpiServiceId> DecodeSpiBearer(std::span<const uint8_t> data)
{
  if (data.empty())
    return {};

  const bool hasEnsemble = (data[0] & 0x40) != 0;
  SpiServiceId id;
  id.isLongSid = (data[0] & 0x10) != 0;
  id.scids = data[0] & 0x0F;
  data = data.subspan(1);

  if (hasEnsemble)
  {
    if (data.size() < 3)
      return {};
    id.ecc = data[0];
    id.eid = static_cast<uint16_t>(ReadUint(data.subspan(1, 2)));
    data = data.subspan(3);
  }

  const size_t sidSize = id.isLongSid ? 4 : 2;
  if (data.size() < sidSize)
    return {};

  id.sid = ReadUint(data.first(sidSize));
  return id;
}

std::optional<std::time_t> DecodeSpiTime(std::span<const uint8_t> data)
{
  if (data.size() < 4)
    return {};

  const uint32_t value = ReadUint(data.first(4));
  const std::time_t mjd = (value >> 14) & 0x1FFFF;
  const bool hasSeconds = (value & 0x800) != 0;
  const int hours = (value >> 6) & 0x1F;
  const int minutes = value & 0x3F;
  int seconds{0};
  if (hasSeconds)
  {
    if (data.size() < 6)
      return {};
    seconds = data[4] >> 2;
  }

  return (mjd - MJD_UNIX_EPOCH) * 86400 + hours * 3600 + minutes * 60 + seconds;
}

std::optional<std::vector<SpiService>> DecodeServiceInformation(std::span<const uint8_t> data)
{
  const auto root = Parse(data, TAG::SERVICE_INFORMATION);
  if (!root)
    return {};

  const CStringDecoder strings(*root);
  std::vector<SpiService> services;
  CollectServices(*root, strings, services);
  return services;
}

std::optional<SpiSchedule> DecodeProgrammeInformation(std::span<const uint8_t> data)
{
  const auto root = Parse(data, TAG::EPG);
  if (!root)
    return {};

  const Element* schedule = root->FindChild(TAG::SCHEDULE);
  if (!schedule)
    return {};

  const CStringDecoder strings(*root);
  SpiSchedule result;
  if (const Element* scope = schedule->FindChild(TAG::SCOPE))
  {
    const auto start = scope->GetAttribute(ATTRIBUTE::SCOPE_START);
    const auto stop = scope->GetAttribute(ATTRIBUTE::SCOPE_STOP);
    const auto startTime = start ? DecodeSpiTime(*start) : std::nullopt;
    const auto stopTime = stop ? DecodeSpiTime(*stop) : std::nullopt;
    if (startTime && stopTime)
    {
      result.scopeStart = *startTime;
      result.scopeEnd = *stopTime;
    }

    for (const auto& child : scope->children)
    {
      const auto id =
          child.tag == TAG::SERVICE_SCOPE ? child.GetAttribute(ATTRIBUTE::ID) : std::nullopt;
      const auto serviceId = id ? DecodeSpiBearer(*id) : std::nullopt;
      if (serviceId)
        result.services.emplace_back(*serviceId);
    }
  }

  for (const auto& child : schedule->children)
  {
    if (child.tag != TAG::PROGRAMME)
      continue;

    if (auto programme = GetProgramme(child, strings))
      result.programmes.emplace_back(std::move(*programme));
  }
  return result;
}

} // namespace DABPLUS
