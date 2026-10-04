/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "DynamicLabel.h"

#include <algorithm>

namespace DABPLUS
{

namespace
{

constexpr uint8_t COMMAND_CLEAR_DISPLAY = 0b0001;
constexpr uint8_t COMMAND_DL_PLUS = 0b0010;

constexpr uint8_t DL_PLUS_TAGS_COMMAND = 0;

constexpr uint8_t CONTENT_ITEM_TITLE = 1;
constexpr uint8_t CONTENT_ITEM_ALBUM = 2;
constexpr uint8_t CONTENT_ITEM_ARTIST = 4;

// DL Plus markers count characters, the label is UTF-8
std::string Utf8Substring(std::string_view text, size_t start, size_t length)
{
  size_t character{0};
  size_t begin{text.size()};
  size_t end{text.size()};
  for (size_t i = 0; i < text.size(); ++i)
  {
    if ((static_cast<uint8_t>(text[i]) & 0xC0) == 0x80)
      continue;

    if (character == start)
      begin = i;
    if (character == start + length)
    {
      end = i;
      break;
    }
    ++character;
  }
  return std::string{text.substr(begin, end - begin)};
}

} // unnamed namespace

bool CDynamicLabelDecoder::ProcessLabel(uint8_t toggle, std::string_view text)
{
  if (toggle == m_labelToggle && text == m_label.text)
    return false;

  // Tags may arrive before their label message is complete; older tags waiting with the same
  // toggle flag refer to a previous message
  std::optional<DlPlusTags> tags = std::move(m_pendingTags);
  m_pendingTags.reset();

  m_labelToggle = toggle;
  ProgrammeLabel label = m_label;
  label.text = text;
  if (tags && tags->link == toggle)
    ApplyTags(tags->command, label);

  return Update(std::move(label));
}

bool CDynamicLabelDecoder::ProcessCommand(std::span<const uint8_t> dataGroup)
{
  if (dataGroup.size() < 2)
    return false;

  const uint8_t command = dataGroup[0] & 0x0F;
  if (command == COMMAND_CLEAR_DISPLAY)
  {
    m_itemToggle = -1;
    m_pendingTags.reset();
    return Update({});
  }

  if (command != COMMAND_DL_PLUS || !AssembleDlPlusCommand(dataGroup))
    return false;

  DlPlusTags tags{m_commandLink, std::move(m_command)};
  m_command.clear();
  if (tags.command.empty() || (tags.command[0] >> 4) != DL_PLUS_TAGS_COMMAND)
    return false;

  if (tags.link != m_labelToggle)
  {
    m_pendingTags = std::move(tags);
    return false;
  }

  ProgrammeLabel label = m_label;
  ApplyTags(tags.command, label);
  return Update(std::move(label));
}

bool CDynamicLabelDecoder::Update(ProgrammeLabel label)
{
  if (label == m_label)
    return false;

  m_label = std::move(label);
  return true;
}

bool CDynamicLabelDecoder::AssembleDlPlusCommand(std::span<const uint8_t> dataGroup)
{
  const bool isFirst = (dataGroup[0] & 0x40) != 0;
  const bool isLast = (dataGroup[0] & 0x20) != 0;
  const uint8_t link = dataGroup[1] >> 7;
  const uint8_t segment = (dataGroup[1] >> 4) & 0x07;
  const size_t length = (dataGroup[1] & 0x0F) + 1u;
  if (dataGroup.size() < 2 + length)
    return false;

  if (isFirst)
  {
    m_command.clear();
    m_commandLink = link;
    m_nextSegment = 1;
  }
  else if (m_command.empty() || segment != m_nextSegment || link != m_commandLink)
  {
    m_command.clear();
    return false;
  }
  else
  {
    ++m_nextSegment;
  }

  m_command.insert(m_command.end(), dataGroup.begin() + 2, dataGroup.begin() + 2 + length);
  return isLast;
}

void CDynamicLabelDecoder::ApplyTags(const std::vector<uint8_t>& command, ProgrammeLabel& label)
{
  const int itemToggle = (command[0] >> 3) & 0x01;
  const bool itemRunning = (command[0] & 0x04) != 0;
  const size_t tagCount = (command[0] & 0x03) + 1u;
  if (command.size() < 1 + 3 * tagCount)
    return;

  if (itemToggle != m_itemToggle || !itemRunning)
  {
    label.title.clear();
    label.artist.clear();
    label.album.clear();
  }
  m_itemToggle = itemToggle;

  if (itemRunning)
  {
    for (size_t tag = 0; tag < tagCount; ++tag)
    {
      const uint8_t contentType = command[1 + 3 * tag] & 0x7F;
      const size_t start = command[2 + 3 * tag] & 0x7F;
      const size_t length = (command[3 + 3 * tag] & 0x7F) + 1u;

      std::string* field{nullptr};
      if (contentType == CONTENT_ITEM_TITLE)
        field = &label.title;
      else if (contentType == CONTENT_ITEM_ARTIST)
        field = &label.artist;
      else if (contentType == CONTENT_ITEM_ALBUM)
        field = &label.album;

      if (field)
        *field = Utf8Substring(label.text, start, length);
    }
  }
}

} // namespace DABPLUS
