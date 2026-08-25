/**
 * @file    biquad_design_test.cpp
 * @brief   Host tests for ADAU biquad design and fixpoint conversion.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BiquadDesign.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] int runUnityGainFixpointTest()
{
    const auto gain = core::GainDb::zero();
    const std::int32_t fix = core::gainDbToLinearFixpoint(gain);
    if (fix != 0x00800000) {
        std::cerr << "expected 0 dB fixpoint 0x00800000, got 0x"
                  << std::hex << fix << std::dec << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runFlatBiquadTest()
{
    const core::BiquadCoefficients flat = core::designFlatEq();
    if (std::fabs(flat.b0 - 1.0F) > 0.001F || std::fabs(flat.b1) > 0.001F
        || std::fabs(flat.b2) > 0.001F || std::fabs(flat.a0) > 0.001F
        || std::fabs(flat.a1) > 0.001F) {
        std::cerr << "flat biquad coefficients unexpected\n";
        return EXIT_FAILURE;
    }

    const auto center = core::FrequencyHz::tryFromHz(1000U);
    const auto zero = core::GainDb::zero();
    const core::BiquadCoefficients peaking =
        core::designPeakingEq(*center, zero, 1.414F);
    if (std::fabs(peaking.b0 - 1.0F) > 0.001F) {
        std::cerr << "0 dB peaking should match flat b0\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * The ADAU1701 Param EQ cell's A0/A1 registers ADD the feedback terms
 * (y = ... + A0 y[n-1] + A1 y[n-2]), unlike the RBJ cookbook's native
 * subtractive form. Denominator 1 - A0 z^-1 - A1 z^-2 = 0 has poles at the
 * roots of z^2 - A0 z - A1 = 0; by the Jury test for a monic real quadratic
 * z^2 + c1 z + c0 (c1 = -A0, c0 = -A1), both poles lie strictly inside the
 * unit circle iff |A1| < 1, A0 + A1 < 1, and A0 - A1 > -1. A missing
 * negation when mapping the RBJ a1/a2 into A0/A1 (2026-08-25 total-silence
 * regression) fails this for real gain/Q combinations, so this guards
 * against that class of bug rather than just checking magnitudes.
 */
[[nodiscard]] bool isAdau1701Stable(const core::BiquadCoefficients &c) noexcept
{
    return std::fabs(c.a1) < 1.0F && (c.a0 + c.a1) < 1.0F
        && (c.a0 - c.a1) > -1.0F;
}

[[nodiscard]] int runPeakingStabilityTest()
{
    const struct
    {
        std::uint32_t hz;
        float dbGain;
        float q;
    } cases[] = {
        {100U, 9.0F, 0.9F},   // bass-enhance band 1 at max level
        {400U, 3.0F, 1.0F},   // bass-enhance band 2 at max level
        {1000U, -1.5F, 1.0F}, // stereo-enhance band 3 at max level
        {3000U, 2.0F, 1.0F},  // stereo-enhance band 4 at max level
        {8000U, 4.0F, 1.0F},  // stereo-enhance band 5 at max level
        {1000U, 12.0F, 10.0F}, // GainDb::kMaxDb at high Q
        {1000U, -96.0F, 0.2F}, // near GainDb::kMinDb at low Q
    };

    for (const auto &tc : cases) {
        const auto center = core::FrequencyHz::tryFromHz(tc.hz);
        const auto gain = core::GainDb::tryFromDb(tc.dbGain);
        const core::BiquadCoefficients peaking =
            core::designPeakingEq(*center, *gain, tc.q);
        if (!isAdau1701Stable(peaking)) {
            std::cerr << "unstable ADAU1701 biquad for " << tc.hz << " Hz, "
                      << tc.dbGain << " dB, Q=" << tc.q << ": a0=" << peaking.a0
                      << " a1=" << peaking.a1 << '\n';
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runUnityGainFixpointTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runFlatBiquadTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runPeakingStabilityTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
