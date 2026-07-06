/**
 * @file    StationName.cpp
 * @brief   StationName implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/StationName.hpp"

namespace core {

std::expected<StationName, ParseError> StationName::tryFrom(std::string_view raw)
{
    if (raw.empty() || raw.size() > kMaxLength) {
        return std::unexpected(ParseError::MissingField);
    }
    return StationName(std::string(raw));
}

StationName::StationName(std::string name)
    : name_(std::move(name))
{
}

std::string_view StationName::value() const noexcept
{
    return name_;
}

} // namespace core
