/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "device/IqFileSource.h"

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

using namespace DABPLUS;

TEST(IqFileSource, AvailableIfFileExists)
{
  const auto path = std::filesystem::temp_directory_path() / "dabplus_iqfilesource_test.iq";
  std::ofstream(path, std::ios::binary) << "IQIQ";

  EXPECT_EQ(CIqFileSource(path.string()).Probe(), OpenResult::OPENED);

  std::filesystem::remove(path);
  EXPECT_EQ(CIqFileSource(path.string()).Probe(), OpenResult::NOT_FOUND);
}
