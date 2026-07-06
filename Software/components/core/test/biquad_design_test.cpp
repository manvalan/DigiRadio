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

} // namespace

int main()
{
    if (runUnityGainFixpointTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runFlatBiquadTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
