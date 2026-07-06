/**
 * @file    PresetSlot.cpp
 * @brief   PresetSlot implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/PresetSlot.hpp"

namespace core {

std::expected<PresetSlot, ParseError> PresetSlot::tryFrom(unsigned slot) noexcept
{
    if (slot < 1U || slot > kMaxSlot) {
        return std::unexpected(ParseError::MissingField);
    }
    return PresetSlot(static_cast<std::uint8_t>(slot));
}

PresetSlot::PresetSlot(std::uint8_t slot) noexcept
    : slot_(slot)
{
}

std::uint8_t PresetSlot::value() const noexcept
{
    return slot_;
}

} // namespace core
