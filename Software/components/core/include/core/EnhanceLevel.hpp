/**
 * @file    EnhanceLevel.hpp
 * @brief   Strong type for 0–100 enhancement intensity (stereo / bass).
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
 * @brief    EnhanceLevel — validated enhancement intensity (0 = off, 100 = max).
 *
 * @dname    EnhanceLevel
 * @return   n/a (type)
 * @pubstate Owns level_ in 0..100 after construction.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class EnhanceLevel {
public:
    /** Maximum enhancement intensity. */
    static constexpr std::uint8_t kMax = 100U;

    /**
     * @brief    tryFromLevel — validate an enhancement level at the boundary.
     *
     * @dname    tryFromLevel
     * @param    level  Untrusted 0..100 value from JSON.
     * @return   EnhanceLevel on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<EnhanceLevel, ParseError> tryFromLevel(
        std::uint32_t level) noexcept;

    /**
     * @brief    zero — enhancement disabled.
     *
     * @dname    zero
     * @return   EnhanceLevel at 0.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static EnhanceLevel zero() noexcept;

    /**
     * @brief    value — read the stored level.
     *
     * @dname    value
     * @return   Level 0..100.
     * @pubstate reads level_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

    /**
     * @brief    fraction — normalised intensity in 0.0..1.0.
     *
     * @dname    fraction
     * @return   level / 100 as float.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] float fraction() const noexcept;

private:
    explicit EnhanceLevel(std::uint8_t level) noexcept;

    std::uint8_t level_;
};

} // namespace core
