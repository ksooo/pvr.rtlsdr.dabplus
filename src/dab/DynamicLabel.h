/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace DABPLUS
{

struct ProgrammeLabel
{
  //! The dynamic label message as received
  std::string text;
  //! The current item as tagged by DL Plus, if any
  std::string title;
  std::string artist;
  std::string album;

  bool HasItem() const { return !title.empty() || !artist.empty() || !album.empty(); }
  bool operator==(const ProgrammeLabel& other) const = default;
};

/*!
 * \brief Combines dynamic label messages and commands (ETSI EN 300 401, 7.4.5.2) with DL Plus
 * tags (ETSI TS 102 980) into programme metadata.
 */
class CDynamicLabelDecoder
{
public:
  /*!
   * \param toggle the toggle flag of the label message
   * \return true if the programme label changed
   */
  bool ProcessLabel(uint8_t toggle, std::string_view text);

  /*!
   * \param dataGroup the command data group without CRC
   * \return true if the programme label changed
   */
  bool ProcessCommand(std::span<const uint8_t> dataGroup);

  const ProgrammeLabel& GetLabel() const { return m_label; }

private:
  struct DlPlusTags
  {
    //! The toggle flag of the label message the tags refer to
    uint8_t link{0};
    std::vector<uint8_t> command;
  };

  bool AssembleDlPlusCommand(std::span<const uint8_t> dataGroup);
  bool Update(ProgrammeLabel label);
  void ApplyTags(const std::vector<uint8_t>& command, ProgrammeLabel& label);

  ProgrammeLabel m_label;
  int m_labelToggle{-1};

  std::vector<uint8_t> m_command;
  uint8_t m_commandLink{0};
  uint8_t m_nextSegment{0};

  //! Tags received before the label message they refer to
  std::optional<DlPlusTags> m_pendingTags;
  int m_itemToggle{-1};
};

} // namespace DABPLUS
