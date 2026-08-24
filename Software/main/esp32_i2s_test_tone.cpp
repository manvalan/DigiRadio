/**
 * @file    esp32_i2s_test_tone.cpp
 * @brief   esp32_i2s_test_tone implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp32_i2s_test_tone.hpp"

#include "esp32_i2s_sink.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>

namespace esp32_i2s_test_tone {

namespace {
constexpr int kSampleRateHz = 48000;
constexpr int kFramesPerBlock = 256;
/** writeSamples() wants top-aligned 24-bit in a 32-bit slot -- 8-bit shift. */
constexpr int kTopAlignShift = 8;
/** Modest amplitude: audible but not a full-scale blast. */
constexpr float kAmplitude = 0.4F * 8388607.0F;
constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;
} // namespace

[[noreturn]] void run(float freqHz)
{
    esp32_i2s_sink::tryAcquire();

    const float phaseStep = kTwoPi * freqHz / static_cast<float>(kSampleRateHz);
    float phase = 0.0F;
    std::int32_t buf[kFramesPerBlock * 2];

    for (;;)
    {
        for (int i = 0; i < kFramesPerBlock; ++i)
        {
            phase += phaseStep;
            if (phase > kTwoPi)
            {
                phase -= kTwoPi;
            }
            const auto sample =
                static_cast<std::int32_t>(std::sin(phase) * kAmplitude)
                << kTopAlignShift;
            buf[(i * 2) + 0] = sample;
            buf[(i * 2) + 1] = sample;
        }
        esp32_i2s_sink::writeSamples(buf, kFramesPerBlock * 2);
    }
}

} // namespace esp32_i2s_test_tone
