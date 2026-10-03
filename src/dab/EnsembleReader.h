/*
 *  Copyright (C) 2026 ksooo
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include "dab/ServiceInfo.h"

#include <cstdint>
#include <optional>

struct DAB_Database;

namespace DABPLUS
{

/*!
 * \brief Extracts the ensemble and its audio services from DAB-Radio's database.
 * \return nothing until the ensemble label has been received
 */
std::optional<EnsembleInfo> ReadEnsemble(const DAB_Database& database, uint32_t frequency);

} // namespace DABPLUS
