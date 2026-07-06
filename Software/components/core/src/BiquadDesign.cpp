/**
 * @file    BiquadDesign.cpp
 * @brief   Biquad coefficient design and ADAU fixpoint helpers.
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

#include <cmath>
#include <numbers>

namespace core {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;

[[nodiscard]] float clampQ(float q) noexcept
{
    if (q < 0.2F) {
        return 0.2F;
    }
    if (q > 10.0F) {
        return 10.0F;
    }
    return q;
}

} // namespace

std::int32_t floatToFixpoint823(float value) noexcept
{
    const double scaled =
        static_cast<double>(value) * static_cast<double>(1U << 23);
    const auto rounded = static_cast<std::int64_t>(std::llround(scaled));
    return static_cast<std::int32_t>(rounded);
}

std::int32_t gainDbToLinearFixpoint(GainDb gain) noexcept
{
    return floatToFixpoint823(gain.linear());
}

BiquadCoefficients designFlatEq() noexcept
{
    return BiquadCoefficients{
        .b0 = 1.0F,
        .b1 = 0.0F,
        .b2 = 0.0F,
        .a0 = 0.0F,
        .a1 = 0.0F,
    };
}

BiquadCoefficients designPeakingEq(FrequencyHz center, GainDb gain,
                                   float q) noexcept
{
    if (std::fabs(gain.value()) < 0.001F) {
        return designFlatEq();
    }

    const float qClamped = clampQ(q);
    const float fs = static_cast<float>(kAdauSampleRateHz);
    const float f0 = static_cast<float>(center.value());
    const float a = std::pow(10.0F, gain.value() / 40.0F);
    const float omega = 2.0F * kPi * f0 / fs;
    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);
    const float alpha = sinOmega / (2.0F * qClamped);

    const float b0 = 1.0F + alpha * a;
    const float b1 = -2.0F * cosOmega;
    const float b2 = 1.0F - alpha * a;
    const float a0 = 1.0F + alpha / a;
    const float a1 = -2.0F * cosOmega;
    const float a2 = 1.0F - alpha / a;

    return BiquadCoefficients{
        .b0 = b0 / a0,
        .b1 = b1 / a0,
        .b2 = b2 / a0,
        .a0 = a1 / a0,
        .a1 = a2 / a0,
    };
}

} // namespace core
