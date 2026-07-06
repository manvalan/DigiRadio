/**
 * @file    EqBandIndex.cpp
 * @brief   EqBandIndex implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/EqBandIndex.hpp"

namespace core {

EqBandIndex::EqBandIndex(std::uint8_t index) noexcept
    : index_(index)
{
}

std::expected<EqBandIndex, ParseError> EqBandIndex::tryFromIndex(
    std::uint32_t index) noexcept
{
    if (index >= kBandCount) {
        return std::unexpected(ParseError::MissingField);
    }
    return EqBandIndex(static_cast<std::uint8_t>(index));
}

std::uint8_t EqBandIndex::value() const noexcept
{
    return index_;
}

} // namespace core
