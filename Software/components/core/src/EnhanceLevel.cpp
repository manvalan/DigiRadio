/**
 * @file    EnhanceLevel.cpp
 * @brief   EnhanceLevel implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/EnhanceLevel.hpp"

namespace core {

EnhanceLevel::EnhanceLevel(std::uint8_t level) noexcept
    : level_(level)
{
}

std::expected<EnhanceLevel, ParseError> EnhanceLevel::tryFromLevel(
    std::uint32_t level) noexcept
{
    if (level > kMax) {
        return std::unexpected(ParseError::MissingField);
    }
    return EnhanceLevel(static_cast<std::uint8_t>(level));
}

EnhanceLevel EnhanceLevel::zero() noexcept
{
    return EnhanceLevel(0U);
}

std::uint8_t EnhanceLevel::value() const noexcept
{
    return level_;
}

float EnhanceLevel::fraction() const noexcept
{
    return static_cast<float>(level_) / static_cast<float>(kMax);
}

} // namespace core
