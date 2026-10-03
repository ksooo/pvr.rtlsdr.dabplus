/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "Id3Tag.h"

#include <string_view>

namespace DABPLUS
{

namespace
{

constexpr uint8_t ENCODING_UTF8 = 0x03;

// ID3v2.4 sizes use 7 bits per byte
void AppendSynchsafe(std::vector<uint8_t>& out, uint32_t value)
{
  out.push_back(static_cast<uint8_t>((value >> 21) & 0x7F));
  out.push_back(static_cast<uint8_t>((value >> 14) & 0x7F));
  out.push_back(static_cast<uint8_t>((value >> 7) & 0x7F));
  out.push_back(static_cast<uint8_t>(value & 0x7F));
}

void AppendTextFrame(std::vector<uint8_t>& out, std::string_view id, std::string_view text)
{
  out.insert(out.end(), id.begin(), id.end());
  AppendSynchsafe(out, static_cast<uint32_t>(1 + text.size()));
  out.push_back(0); // flags
  out.push_back(0);
  out.push_back(ENCODING_UTF8);
  out.insert(out.end(), text.begin(), text.end());
}

} // unnamed namespace

std::vector<uint8_t> CreateId3Tag(const Id3Fields& fields)
{
  std::vector<uint8_t> frames;
  AppendTextFrame(frames, "TIT2", fields.title);
  AppendTextFrame(frames, "TPE1", fields.artist);
  AppendTextFrame(frames, "TALB", fields.album);
  AppendTextFrame(frames, "TCON", fields.genre);

  std::vector<uint8_t> tag{'I', 'D', '3', 0x04, 0x00, 0x00};
  AppendSynchsafe(tag, static_cast<uint32_t>(frames.size()));
  tag.insert(tag.end(), frames.begin(), frames.end());
  return tag;
}

} // namespace DABPLUS
