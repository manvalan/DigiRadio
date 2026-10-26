/**
 * @file    pcm_resampler_test.cpp
 * @brief   Host tests for PcmResampler and driftTrimPpm.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/PcmResampler.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

/** Converts `seconds` of a sine at `toneHz` in blocks of 1152 frames, the
 *  size of one MP3 frame, and returns the 48 kHz result (left channel). */
[[nodiscard]] std::vector<std::int16_t> convertTone(std::uint32_t rateHz,
                                                    double toneHz,
                                                    double seconds)
{
    core::PcmResampler resampler;
    if (!resampler.setInputRate(rateHz)) {
        return {};
    }
    const auto total = static_cast<std::size_t>(rateHz * seconds);
    std::vector<std::int16_t> left;
    std::vector<std::int16_t> block;
    std::vector<std::int16_t> out;
    for (std::size_t start = 0U; start < total; start += 1152U) {
        block.clear();
        for (std::size_t i = start; i < std::min(total, start + 1152U); ++i) {
            const double v = 16000.0 * std::sin(2.0 * kPi * toneHz * i / rateHz);
            block.push_back(static_cast<std::int16_t>(v));
            block.push_back(static_cast<std::int16_t>(-v));
        }
        out.assign(resampler.maxOutputFrames(block.size() / 2U) * 2U, 0);
        const std::size_t frames = resampler.process(block, out);
        for (std::size_t i = 0U; i < frames; ++i) {
            left.push_back(out[i * 2U]);
            if (out[i * 2U + 1U] != static_cast<std::int16_t>(-out[i * 2U])
                && std::abs(out[i * 2U + 1U] + out[i * 2U]) > 1) {
                return {}; // channels must stay independent and mirrored
            }
        }
    }
    return left;
}

/** Largest difference between the output and an ideal 48 kHz sine. */
[[nodiscard]] double worstError(const std::vector<std::int16_t>& left,
                                double toneHz)
{
    double worst = 0.0;
    // Skip the start: the converter has a two-frame history delay.
    for (std::size_t i = 16U; i < left.size(); ++i) {
        const double t = (static_cast<double>(i) - 2.0 * 48000.0 / 44100.0)
                         / 48000.0;
        const double ideal = 16000.0 * std::sin(2.0 * kPi * toneHz * t);
        worst = std::max(worst, std::abs(ideal - left[i]));
    }
    return worst;
}

[[nodiscard]] int runRateTest()
{
    const auto left = convertTone(44100U, 1000.0, 1.0);
    // One second in must give 48 000 frames out, within the history delay.
    if (left.size() < 47995U || left.size() > 48003U) {
        std::cerr << "44.1k -> 48k produced " << left.size() << " frames\n";
        return EXIT_FAILURE;
    }
    const double error = worstError(left, 1000.0);
    if (error > 200.0) { // < -38 dB of a -6 dBFS tone at 1 kHz
        std::cerr << "1 kHz tone error too large: " << error << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runPassThroughTest()
{
    core::PcmResampler resampler;
    if (!resampler.setInputRate(48000U)) {
        return EXIT_FAILURE;
    }
    std::vector<std::int16_t> in;
    for (std::int16_t i = 1; i <= 8; ++i) {
        in.push_back(i);
        in.push_back(static_cast<std::int16_t>(i * 10));
    }
    std::vector<std::int16_t> out(resampler.maxOutputFrames(8U) * 2U);
    const std::size_t frames = resampler.process(in, out);
    // 48k -> 48k is a pure two-frame delay: 0, 0, 1, 2, ... 6.
    if (frames != 8U || out[4] != 1 || out[5] != 10 || out[14] != 6) {
        std::cerr << "pass-through mismatch (" << frames << " frames)\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTrimAndRangeTest()
{
    core::PcmResampler resampler;
    if (resampler.setInputRate(4000U) || resampler.setInputRate(200000U)) {
        std::cerr << "out-of-range rates must be refused\n";
        return EXIT_FAILURE;
    }
    if (core::driftTrimPpm(50U) != 0 || core::driftTrimPpm(100U) != 300
        || core::driftTrimPpm(0U) != -300 || core::driftTrimPpm(60U) != 60) {
        std::cerr << "driftTrimPpm mapping mismatch\n";
        return EXIT_FAILURE;
    }
    // +1000 ppm must consume input faster: fewer output frames.
    if (!resampler.setInputRate(48000U)) {
        return EXIT_FAILURE;
    }
    resampler.setTrimPpm(1000);
    std::vector<std::int16_t> in(2U * 10000U, 0);
    std::vector<std::int16_t> out(resampler.maxOutputFrames(10000U) * 2U);
    const std::size_t frames = resampler.process(in, out);
    if (frames < 9988U || frames > 9993U) {
        std::cerr << "trimmed output frames " << frames << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runRateTest() != EXIT_SUCCESS || runPassThroughTest() != EXIT_SUCCESS
        || runTrimAndRangeTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    std::cout << "pcm_resampler_test OK\n";
    return EXIT_SUCCESS;
}
