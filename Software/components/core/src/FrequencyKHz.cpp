/**
 * @file    FrequencyKHz.cpp
 * @brief   FrequencyKHz implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FrequencyKHz.hpp"

namespace core {

FrequencyKHz::FrequencyKHz(std::uint32_t khz) noexcept
    : khz_(khz)
{
}

std::expected<FrequencyKHz, ParseError> FrequencyKHz::tryFromKhz(
    std::uint32_t khz) noexcept
{
    if (khz < kMinKhz || khz > kMaxKhz) {
        return std::unexpected(ParseError::MissingField);
    }
    return FrequencyKHz(khz);
}

std::uint32_t FrequencyKHz::value() const noexcept
{
    return khz_;
}

} // namespace core
