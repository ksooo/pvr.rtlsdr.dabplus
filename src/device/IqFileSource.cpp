/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "IqFileSource.h"

#include "utils/Log.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <utility>

namespace DABPLUS
{

namespace
{

constexpr size_t BLOCK_SIZE = 65536;

} // unnamed namespace

CIqFileSource::CIqFileSource(std::string path) : m_path(std::move(path))
{
}

CIqFileSource::~CIqFileSource()
{
  Close();
}

OpenResult CIqFileSource::Probe() const
{
  return std::ifstream(m_path, std::ios::binary).good() ? OpenResult::OPENED
                                                        : OpenResult::NOT_FOUND;
}

OpenResult CIqFileSource::Open()
{
  std::ifstream file(m_path, std::ios::binary);
  if (!file)
  {
    Log(LogLevel::LEVEL_ERROR, "Unable to open I/Q recording '{}'", m_path);
    return OpenResult::NOT_FOUND;
  }

  m_samples.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  m_samples.resize(m_samples.size() & ~size_t{1});
  return m_samples.empty() ? OpenResult::FAILED : OpenResult::OPENED;
}

void CIqFileSource::Close()
{
  Stop();
  m_samples.clear();
}

bool CIqFileSource::Start(SamplesCallback callback)
{
  if (m_samples.empty() || m_thread.joinable())
    return false;

  m_stop = false;
  m_thread = std::thread(
      [this, callback = std::move(callback)]
      {
        using namespace std::chrono;
        const auto start = steady_clock::now();
        uint64_t delivered{0};
        size_t position{0};
        while (!m_stop)
        {
          const size_t length = std::min(BLOCK_SIZE, m_samples.size() - position);
          callback({m_samples.data() + position, length});
          delivered += length;
          position = (position + length) % m_samples.size();

          // Two bytes per sample
          const auto due = start + microseconds(delivered * 1000000 / (2 * DAB_SAMPLE_RATE));
          std::this_thread::sleep_until(due);
        }
      });
  return true;
}

void CIqFileSource::Stop()
{
  if (!m_thread.joinable())
    return;

  m_stop = true;
  m_thread.join();
}

} // namespace DABPLUS
