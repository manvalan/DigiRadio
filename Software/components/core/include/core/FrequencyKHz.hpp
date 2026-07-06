/**
 * @file    FrequencyKHz.hpp
 * @brief   Strong type for FM centre frequency in kilohertz.
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
 * @brief    FrequencyKHz — validated FM centre frequency (European band).
 *
 * @dname    FrequencyKHz
 * @return   n/a (type)
 * @pubstate Owns khz_ within 64\,000–108\,000 kHz. Immutable after construction.
 *
 * Parsed once at the HTTP boundary; trusted downstream by ITuner and drivers.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class FrequencyKHz {
public:
    /** Minimum valid FM frequency for DigiRadio (64.0 MHz). */
    static constexpr std::uint32_t kMinKhz = 64000U;
    /** Maximum valid FM frequency for DigiRadio (108.0 MHz). */
    static constexpr std::uint32_t kMaxKhz = 108000U;

    /**
     * @brief    tryFromKhz — validate an FM frequency at the boundary.
     *
     * @dname    tryFromKhz
     * @param    khz  Untrusted frequency in kilohertz from JSON input.
     * @return   FrequencyKHz on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<FrequencyKHz, ParseError> tryFromKhz(
        std::uint32_t khz) noexcept;

    /**
     * @brief    value — read the stored frequency in kilohertz.
     *
     * @dname    value
     * @return   Validated centre frequency in kHz.
     * @pubstate reads khz_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint32_t value() const noexcept;

private:
    explicit FrequencyKHz(std::uint32_t khz) noexcept;

    std::uint32_t khz_;
};

} // namespace core
