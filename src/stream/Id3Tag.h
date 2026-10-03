/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DABPLUS
{

struct Id3Fields
{
  std::string title;
  std::string artist;
  std::string album;
  std::string genre;

  bool operator==(const Id3Fields& other) const = default;
};

/*!
 * \brief Creates an ID3v2.4 tag with UTF-8 text frames TIT2, TPE1, TALB and TCON. Empty fields
 * are written as empty frames, so that a receiver clears values of a previous tag.
 */
std::vector<uint8_t> CreateId3Tag(const Id3Fields& fields);

} // namespace DABPLUS
