/**
 * @file    FrequencyHz.hpp
 * @brief   Strong type for audio centre frequencies (EQ bands).
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
 * @brief    FrequencyHz — validated centre frequency for parametric EQ.
 *
 * @dname    FrequencyHz
 * @return   n/a (type)
 * @pubstate Owns hz_ within 20..20\,000 Hz (48 kHz SigmaStudio project).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class FrequencyHz {
public:
    /** Minimum EQ centre frequency (Hz). */
    static constexpr std::uint32_t kMinHz = 20U;
    /** Maximum EQ centre frequency (Hz). */
    static constexpr std::uint32_t kMaxHz = 20000U;

    /**
     * @brief    tryFromHz — validate a centre frequency at the boundary.
     *
     * @dname    tryFromHz
     * @param    hz  Untrusted frequency in hertz.
     * @return   FrequencyHz on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<FrequencyHz, ParseError> tryFromHz(
        std::uint32_t hz) noexcept;

    /**
     * @brief    value — read the stored frequency in hertz.
     *
     * @dname    value
     * @return   Validated centre frequency in Hz.
     * @pubstate reads hz_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint32_t value() const noexcept;

private:
    explicit FrequencyHz(std::uint32_t hz) noexcept;

    std::uint32_t hz_;
};

} // namespace core
