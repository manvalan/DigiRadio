/**
 * @file    FrequencyHz.cpp
 * @brief   FrequencyHz implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FrequencyHz.hpp"

namespace core {

FrequencyHz::FrequencyHz(std::uint32_t hz) noexcept
    : hz_(hz)
{
}

std::expected<FrequencyHz, ParseError> FrequencyHz::tryFromHz(
    std::uint32_t hz) noexcept
{
    if (hz < kMinHz || hz > kMaxHz) {
        return std::unexpected(ParseError::MissingField);
    }
    return FrequencyHz(hz);
}

std::uint32_t FrequencyHz::value() const noexcept
{
    return hz_;
}

} // namespace core
