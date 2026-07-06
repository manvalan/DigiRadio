/**
 * @file    GainDb.hpp
 * @brief   Strong type for decibel gain/attenuation values.
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

#include <cmath>
#include <expected>

namespace core {

/**
 * @brief    GainDb — validated gain in decibels for DSP volume and EQ.
 *
 * @dname    GainDb
 * @return   n/a (type)
 * @pubstate Owns db_ within kMinDb..kMaxDb after construction.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class GainDb {
public:
    /** Minimum attenuation (effectively muted on the linear path). */
    static constexpr float kMinDb = -96.0F;
    /** Maximum boost allowed on the DSP path. */
    static constexpr float kMaxDb = 12.0F;

    /**
     * @brief    tryFromDb — validate a gain value at the HTTP boundary.
     *
     * @dname    tryFromDb
     * @param    db  Untrusted gain in decibels.
     * @return   GainDb on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<GainDb, ParseError> tryFromDb(
        float db) noexcept;

    /**
     * @brief    zero — unity gain (0 dB).
     *
     * @dname    zero
     * @return   GainDb at 0 dB.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static GainDb zero() noexcept;

    /**
     * @brief    value — read the stored gain in decibels.
     *
     * @dname    value
     * @return   Validated gain in dB.
     * @pubstate reads db_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] float value() const noexcept;

    /**
     * @brief    linear — convert to linear amplitude (not dB).
     *
     * @dname    linear
     * @return   Linear gain factor (1.0 at 0 dB).
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] float linear() const noexcept;

private:
    explicit GainDb(float db) noexcept;

    float db_;
};

} // namespace core
