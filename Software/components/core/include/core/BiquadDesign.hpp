/**
 * @file    BiquadDesign.hpp
 * @brief   Host-testable biquad coefficient design for ADAU1701 PEQ.
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

#include "core/BiquadCoefficients.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

namespace core {

/** Sample rate of the DigiRadio SigmaStudio project (Hz). */
inline constexpr std::uint32_t kAdauSampleRateHz = 48000U;

/**
 * @brief    designPeakingEq — compute PEQ coefficients for one band.
 *
 * @dname    designPeakingEq
 * @param    center     Validated centre frequency.
 * @param    gain       Band gain in dB (0 dB yields flat response).
 * @param    q          Quality factor (typically 0.5..10).
 * @return   BiquadCoefficients in SigmaStudio Param EQ order.
 * @pubstate none
 *
 * Uses the Robert Bristow-Johnson peaking EQ formulae at kAdauSampleRateHz.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] BiquadCoefficients designPeakingEq(FrequencyHz center, GainDb gain,
                                                 float q) noexcept;

/**
 * @brief    designFlatEq — unity-gain bypass coefficients for a PEQ band.
 *
 * @dname    designFlatEq
 * @return   Identity biquad (b0=1, others 0) as in SigmaStudio defaults.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] BiquadCoefficients designFlatEq() noexcept;

/**
 * @brief    gainDbToLinearFixpoint — map dB to ADAU 8.23 linear gain word.
 *
 * @dname    gainDbToLinearFixpoint
 * @param    gain  Validated gain in dB.
 * @return   32-bit fixpoint suitable for safeload (0 dB = 0x00800000).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::int32_t gainDbToLinearFixpoint(GainDb gain) noexcept;

/**
 * @brief    floatToFixpoint823 — convert a float to ADAU 8.23 fixpoint.
 *
 * @dname    floatToFixpoint823
 * @param    value  Floating value (typically -16..+16 for filter coeffs).
 * @return   Rounded 32-bit fixpoint word.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::int32_t floatToFixpoint823(float value) noexcept;

} // namespace core
