/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceInfo.h"

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

  std::string ToJson() const;

  /*!
   * \return false if the data could not be parsed; the store is unchanged then
   */
  bool FromJson(std::string_view json);

private:
  std::vector<EnsembleInfo> m_ensembles;
};

} // namespace DABPLUS
