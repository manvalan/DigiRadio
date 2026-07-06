/**
 * @file    PresetSlot.hpp
 * @brief   Strong type for a physical preset button slot (1–20).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/ParseError.hpp"

#include <cstdint>
#include <expected>

namespace core {

/**
 * @brief    PresetSlot — optional hardware preset index on the radio.
 *
 * @dname    PresetSlot
 * @return   n/a (type)
 * @pubstate Owns slot_ in [1, kMaxSlot]. Immutable after construction.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class PresetSlot {
public:
    /** Highest preset button index supported by the list model. */
    static constexpr std::uint8_t kMaxSlot = 20U;

    /**
     * @brief    tryFrom — validate a preset slot at the boundary.
     *
     * @dname    tryFrom
     * @param    slot  Untrusted slot number from JSON (1–20).
     * @return   PresetSlot on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<PresetSlot, ParseError>
    tryFrom(unsigned slot) noexcept;

    /**
     * @brief    value — read the slot number.
     *
     * @dname    value
     * @return   Slot index 1–20.
     * @pubstate reads slot_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

private:
    explicit PresetSlot(std::uint8_t slot) noexcept;

    std::uint8_t slot_;
};

} // namespace core
