/**
 * @file    GainDb.cpp
 * @brief   GainDb implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/GainDb.hpp"

namespace core {

GainDb::GainDb(float db) noexcept
    : db_(db)
{
}

std::expected<GainDb, ParseError> GainDb::tryFromDb(float db) noexcept
{
    if (db < kMinDb || db > kMaxDb) {
        return std::unexpected(ParseError::MissingField);
    }
    return GainDb(db);
}

GainDb GainDb::zero() noexcept
{
    return GainDb(0.0F);
}

float GainDb::value() const noexcept
{
    return db_;
}

float GainDb::linear() const noexcept
{
    return std::pow(10.0F, db_ / 20.0F);
}

} // namespace core
