/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceInfo.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace DABPLUS
{

/*!
 * \brief The ensembles found by the last channel scan, and the channels and channel groups derived
 * from them.
 */
class CChannelStore
{
public:
  void SetEnsembles(std::vector<EnsembleInfo> ensembles) { m_ensembles = std::move(ensembles); }
  const std::vector<EnsembleInfo>& GetEnsembles() const { return m_ensembles; }

  /*!
   * \brief All services, each only once even if it is part of several ensembles.
   */
  std::vector<ServiceInfo> GetServices() const;

  /*!
   * \brief The channel group name of an ensemble; unique within the store.
   */
  std::string GetGroupName(const EnsembleInfo& ensemble) const;

  std::optional<ServiceInfo> FindService(int uid) const;
  std::optional<EnsembleInfo> FindEnsemble(uint32_t frequency) const;

  /*!
   * \brief The frequencies of all ensembles carrying the service, the one it was last received
   * on first.
   */
  std::vector<uint32_t> GetFrequencies(int uid) const;
  void SetLastFrequency(int uid, uint32_t frequency) { m_lastFrequencies[uid] = frequency; }

  /*!
   * \brief The file name of the logo of a service; empty if there is none.
   */
  std::string GetLogo(int uid) const;
  void SetLogo(int uid, std::string fileName) { m_logos[uid] = std::move(fileName); }
  bool IsLogoUsed(const std::string& fileName) const;

  std::string ToJson() const;

  /*!
   * \return false if the data could not be parsed; the store is unchanged then
   */
  bool FromJson(std::string_view json);

private:
  std::vector<EnsembleInfo> m_ensembles;
  std::map<int, uint32_t> m_lastFrequencies;
  std::map<int, std::string> m_logos;
};

} // namespace DABPLUS
