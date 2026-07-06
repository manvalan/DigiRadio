/**
 * @file    BiquadCoefficients.hpp
 * @brief   Float and fixpoint biquad coefficients for ADAU1701 PEQ.
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

#include <array>
#include <cstdint>

namespace core {

/**
 * @brief    BiquadCoefficients — five PEQ coefficients (b0..a1).
 *
 * @dname    BiquadCoefficients
 * @return   n/a (type)
 * @pubstate Plain value type matching SigmaStudio Param EQ cell order.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct BiquadCoefficients {
    float b0; ///< Numerator b0.
    float b1; ///< Numerator b1.
    float b2; ///< Numerator b2.
    float a0; ///< Denominator a0 (ADI stores feedback terms).
    float a1; ///< Denominator a1.

    /**
     * @brief    toFixpoint823 — convert each coefficient to 8.23 fixpoint.
     *
     * @dname    toFixpoint823
     * @return   Array of five 32-bit fixpoint words for safeload.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::array<std::int32_t, 5> toFixpoint823() const noexcept;
};

} // namespace core
