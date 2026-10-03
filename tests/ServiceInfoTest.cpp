/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/ServiceInfo.h"

#include <gtest/gtest.h>

using namespace DABPLUS;

TEST(ServiceInfo, UidIsPositiveForLargestIds)
{
  const ServiceInfo service{.ecc = 0xFF, .sid = 0xFFFF, .scids = 0x0F};
  EXPECT_GT(service.GetUid(), 0);
}

TEST(ServiceInfo, UidDistinguishesEccSidAndScids)
{
  const ServiceInfo service{.ecc = 0xE0, .sid = 0xD210, .scids = 0};

  ServiceInfo otherEcc{service};
  otherEcc.ecc = 0xE1;
  ServiceInfo otherSid{service};
  otherSid.sid = 0xD220;
  ServiceInfo otherScids{service};
  otherScids.scids = 1;

  EXPECT_NE(service.GetUid(), otherEcc.GetUid());
  EXPECT_NE(service.GetUid(), otherSid.GetUid());
  EXPECT_NE(service.GetUid(), otherScids.GetUid());
}

TEST(ServiceInfo, UidIgnoresLabels)
{
  const ServiceInfo service{.ecc = 0xE0, .sid = 0xD210, .label = "Dlf"};
  const ServiceInfo renamed{.ecc = 0xE0, .sid = 0xD210, .label = "Deutschlandfunk"};
  EXPECT_EQ(service.GetUid(), renamed.GetUid());
}
