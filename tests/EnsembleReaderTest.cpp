/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "dab/EnsembleReader.h"

#include <string>

#include <dab/database/dab_database.h>
#include <gtest/gtest.h>

using namespace DABPLUS;

namespace
{

constexpr uint32_t FREQUENCY = 178352000;

class EnsembleReaderTest : public testing::Test
{
protected:
  void SetUp() override
  {
    m_database.ensemble.id.value = 0x10BC;
    m_database.ensemble.extended_country_code = 0xE0;
    m_database.ensemble.label = "DR Deutschland  ";
    m_database.ensemble.short_label = "DR D";
    m_database.ensemble.is_complete = true;
  }

  void AddAudioService(uint16_t sid,
                       const std::string& label,
                       AudioServiceType type = AudioServiceType::DAB_PLUS,
                       bool isComplete = true)
  {
    const ServiceId id{sid, ServiceIdType::BITS16};
    Service service{id};
    service.label = label;
    service.short_label = label.substr(0, 3);
    service.programme_type = 3;
    service.is_complete = isComplete;
    m_database.services.push_back(service);

    ServiceComponent component{id, 0};
    component.transport_mode = TransportMode::STREAM_MODE_AUDIO;
    component.audio_service_type = type;
    component.is_complete = true;
    m_database.service_components.push_back(component);
  }

  std::optional<EnsembleInfo> Read() const { return ReadEnsemble(m_database, FREQUENCY); }

  DAB_Database m_database;
};

} // unnamed namespace

TEST_F(EnsembleReaderTest, NothingWithoutEnsembleLabel)
{
  m_database.ensemble.label.clear();
  AddAudioService(0xD210, "Dlf");
  EXPECT_FALSE(Read());
}

TEST_F(EnsembleReaderTest, ReadsEnsembleAndServices)
{
  AddAudioService(0xD210, "Dlf             ");
  AddAudioService(0x15DD, "RADIO BOB!", AudioServiceType::DAB);

  const auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  EXPECT_EQ(ensemble->label, "DR Deutschland");
  EXPECT_EQ(ensemble->ecc, 0xE0);
  EXPECT_EQ(ensemble->eid, 0x10BC);
  EXPECT_EQ(ensemble->frequency, FREQUENCY);
  EXPECT_TRUE(ensemble->isComplete);

  ASSERT_EQ(ensemble->services.size(), 2u);
  const ServiceInfo& dlf = ensemble->services[0];
  EXPECT_EQ(dlf.label, "Dlf");
  EXPECT_EQ(dlf.sid, 0xD210);
  EXPECT_EQ(dlf.ecc, 0xE0);
  EXPECT_EQ(dlf.programmeType, 3);
  EXPECT_TRUE(dlf.isDabPlus);
  EXPECT_FALSE(ensemble->services[1].isDabPlus);
}

TEST_F(EnsembleReaderTest, SortsServicesByLabelIgnoringCase)
{
  AddAudioService(0x1001, "radio horeb");
  AddAudioService(0x1002, "Absolut relax");
  AddAudioService(0x1003, "ENERGY");

  const auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  ASSERT_EQ(ensemble->services.size(), 3u);
  EXPECT_EQ(ensemble->services[0].label, "Absolut relax");
  EXPECT_EQ(ensemble->services[1].label, "ENERGY");
  EXPECT_EQ(ensemble->services[2].label, "radio horeb");
}

TEST_F(EnsembleReaderTest, SkipsDataServices)
{
  AddAudioService(0xD210, "Dlf");

  const ServiceId epgId{0xE0D110BC, ServiceIdType::BITS32};
  Service epg{epgId};
  epg.label = "EPG Deutschland";
  epg.is_complete = true;
  m_database.services.push_back(epg);
  ServiceComponent epgComponent{epgId, 0};
  epgComponent.transport_mode = TransportMode::PACKET_MODE_DATA;
  epgComponent.is_complete = true;
  m_database.service_components.push_back(epgComponent);

  const auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  ASSERT_EQ(ensemble->services.size(), 1u);
  EXPECT_EQ(ensemble->services[0].label, "Dlf");
  EXPECT_TRUE(ensemble->isComplete);
}

TEST_F(EnsembleReaderTest, IncompleteServiceIsLeftOut)
{
  AddAudioService(0xD210, "Dlf");
  AddAudioService(0xD220, "Dlf Kultur", AudioServiceType::DAB_PLUS, false);

  const auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  ASSERT_EQ(ensemble->services.size(), 1u);
  EXPECT_FALSE(ensemble->isComplete);
}

TEST_F(EnsembleReaderTest, IncompleteUntilAllSignalledServicesAreKnown)
{
  m_database.ensemble.nb_services = 2;
  AddAudioService(0xD210, "Dlf");

  auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  EXPECT_FALSE(ensemble->isComplete);

  AddAudioService(0xD220, "Dlf Kultur");
  ensemble = Read();
  ASSERT_TRUE(ensemble);
  EXPECT_TRUE(ensemble->isComplete);
}

TEST_F(EnsembleReaderTest, IncompleteWithoutExtendedCountryCode)
{
  AddAudioService(0xD210, "Dlf");
  m_database.ensemble.extended_country_code = 0;

  const auto ensemble = Read();
  ASSERT_TRUE(ensemble);
  EXPECT_FALSE(ensemble->isComplete);
}
