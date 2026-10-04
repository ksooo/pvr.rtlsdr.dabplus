/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "store/ChannelStore.h"

#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

ServiceInfo MakeService(uint16_t sid, const std::string& label)
{
  return {.ecc = 0xE0,
          .sid = sid,
          .scids = 0,
          .label = label,
          .shortLabel = label.substr(0, 3),
          .programmeType = 10,
          .isDabPlus = true};
}

EnsembleInfo MakeEnsemble(const std::string& label,
                          uint32_t frequency,
                          std::vector<ServiceInfo> services)
{
  return {.ecc = 0xE0,
          .eid = 0x1000,
          .label = label,
          .shortLabel = label.substr(0, 4),
          .frequency = frequency,
          .services = std::move(services)};
}

} // unnamed namespace

TEST(ChannelStore, JsonRoundTrip)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("DR Deutschland", 178352000,
                                   {MakeService(0xD210, "Dlf"), MakeService(0xD220, "Dlf Kultur")}),
                      MakeEnsemble("Antenne DE", 180064000, {MakeService(0x1A45, "ENERGY")})});

  CChannelStore restored;
  ASSERT_TRUE(restored.FromJson(store.ToJson()));

  ASSERT_EQ(restored.GetEnsembles().size(), 2u);
  const EnsembleInfo& ensemble = restored.GetEnsembles()[0];
  EXPECT_EQ(ensemble.label, "DR Deutschland");
  EXPECT_EQ(ensemble.shortLabel, "DR D");
  EXPECT_EQ(ensemble.ecc, 0xE0);
  EXPECT_EQ(ensemble.eid, 0x1000);
  EXPECT_EQ(ensemble.frequency, 178352000u);
  EXPECT_EQ(ensemble.services, store.GetEnsembles()[0].services);
  EXPECT_EQ(restored.GetEnsembles()[1].services, store.GetEnsembles()[1].services);
}

TEST(ChannelStore, InvalidJsonLeavesStoreUnchanged)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("DR Deutschland", 178352000, {MakeService(0xD210, "Dlf")})});

  EXPECT_FALSE(store.FromJson("{ not json"));
  EXPECT_FALSE(store.FromJson(R"({"version": 1, "ensembles": [{"label": "x"}]})"));
  EXPECT_FALSE(store.FromJson(R"({"version": 99, "ensembles": []})"));

  ASSERT_EQ(store.GetEnsembles().size(), 1u);
  EXPECT_EQ(store.GetEnsembles()[0].label, "DR Deutschland");
}

TEST(ChannelStore, ServiceInSeveralEnsemblesIsOneChannel)
{
  CChannelStore store;
  store.SetEnsembles(
      {MakeEnsemble("Hamburg K10D", 215072000, {MakeService(0xD210, "Dlf")}),
       MakeEnsemble("DR Deutschland", 178352000,
                    {MakeService(0xD210, "Dlf"), MakeService(0xD220, "Dlf Kultur")})});

  const auto services = store.GetServices();
  ASSERT_EQ(services.size(), 2u);
  EXPECT_EQ(services[0].label, "Dlf");
  EXPECT_EQ(services[1].label, "Dlf Kultur");
}

TEST(ChannelStore, FrequenciesOfServiceLastReceivedFirst)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("DR Deutschland", 178352000, {MakeService(0xD75B, "KLASSIK")}),
                      MakeEnsemble("Antenne DE", 180064000, {MakeService(0x1A45, "ENERGY")}),
                      MakeEnsemble("Hamburg K10D", 215072000, {MakeService(0xD75B, "KLASSIK")})});
  const int uid = MakeService(0xD75B, "KLASSIK").GetUid();

  EXPECT_EQ(store.GetFrequencies(uid), (std::vector<uint32_t>{178352000, 215072000}));

  store.SetLastFrequency(uid, 215072000);
  EXPECT_EQ(store.GetFrequencies(uid), (std::vector<uint32_t>{215072000, 178352000}));

  CChannelStore restored;
  ASSERT_TRUE(restored.FromJson(store.ToJson()));
  EXPECT_EQ(restored.GetFrequencies(uid), (std::vector<uint32_t>{215072000, 178352000}));
}

TEST(ChannelStore, LastFrequencyOfOtherEnsembleIsIgnored)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("DR Deutschland", 178352000, {MakeService(0xD210, "Dlf")})});
  const int uid = MakeService(0xD210, "Dlf").GetUid();

  store.SetLastFrequency(uid, 222064000);
  EXPECT_EQ(store.GetFrequencies(uid), (std::vector<uint32_t>{178352000}));
}

TEST(ChannelStore, FindServiceAndEnsemble)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("DR Deutschland", 178352000, {MakeService(0xD210, "Dlf")})});

  const auto service = store.FindService(MakeService(0xD210, "Dlf").GetUid());
  ASSERT_TRUE(service);
  EXPECT_EQ(service->label, "Dlf");
  EXPECT_FALSE(store.FindService(MakeService(0xD220, "Dlf Kultur").GetUid()));

  const auto ensemble = store.FindEnsemble(178352000);
  ASSERT_TRUE(ensemble);
  EXPECT_EQ(ensemble->label, "DR Deutschland");
  EXPECT_FALSE(store.FindEnsemble(180064000));
}

TEST(ChannelStore, Logos)
{
  CChannelStore store;
  store.SetEnsembles(
      {MakeEnsemble("DR Deutschland", 178352000,
                    {MakeService(0xD210, "Dlf"), MakeService(0xD220, "Dlf Kultur")})});
  const int dlf = MakeService(0xD210, "Dlf").GetUid();
  const int kultur = MakeService(0xD220, "Dlf Kultur").GetUid();

  EXPECT_TRUE(store.GetLogo(dlf).empty());
  store.SetLogo(dlf, "a.png");
  store.SetLogo(kultur, "a.png");
  store.SetLogo(kultur, "b.png");
  EXPECT_TRUE(store.IsLogoUsed("a.png"));
  EXPECT_FALSE(store.IsLogoUsed("c.png"));

  CChannelStore restored;
  ASSERT_TRUE(restored.FromJson(store.ToJson()));
  EXPECT_EQ(restored.GetLogo(dlf), "a.png");
  EXPECT_EQ(restored.GetLogo(kultur), "b.png");
}

TEST(ChannelStore, GroupNamesAreUnique)
{
  CChannelStore store;
  store.SetEnsembles({MakeEnsemble("Bayern", 178352000, {}), MakeEnsemble("Bayern", 211648000, {}),
                      MakeEnsemble("Antenne DE", 180064000, {})});

  const auto& ensembles = store.GetEnsembles();
  EXPECT_EQ(store.GetGroupName(ensembles[0]), "Bayern (5C)");
  EXPECT_EQ(store.GetGroupName(ensembles[1]), "Bayern (10B)");
  EXPECT_EQ(store.GetGroupName(ensembles[2]), "Antenne DE");
}
