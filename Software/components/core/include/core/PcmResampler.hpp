/**
 * @file    PcmResampler.hpp
 * @brief   Stereo 16-bit PCM sample-rate converter (cubic Hermite).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace core {

/**
 * @brief    PcmResampler — converts interleaved stereo int16 PCM from any
 *           decoder rate to the ADAU1701's fixed 48 kHz I2S rate.
 *
 * The ADAU1701 is I2S master at 48 kHz (12.288 MHz crystal, 256×fs), so
 * every ESP32 producer must deliver exactly 48 000 frames per second.
 * Most MP3 radio streams are 44.1 kHz: written unconverted they play 9 %
 * fast and starve the network buffer. Cubic Hermite interpolation over
 * four frames, block by block with a three-frame history so block edges
 * are seamless. A small trim (ppm) lets the caller nudge the ratio to
 * absorb the drift between the server's clock and the ADAU crystal.
 *
 * @dname    PcmResampler
 * @pubstate holds the conversion step, phase and three history frames.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class PcmResampler {
public:
    /** Output rate the ADAU1701 consumes. */
    static constexpr std::uint32_t kOutputRateHz = 48000U;
    /** Largest trim accepted by setTrimPpm(), either sign. */
    static constexpr std::int32_t kMaxTrimPpm = 1000;

    /**
     * @brief    setInputRate — configure for a new source rate, clearing
     *           history.
     *
     * @dname    setInputRate
     * @param    inputRateHz  Decoder output rate (8000–96000 Hz).
     * @return   false (unchanged) when the rate is out of range.
     * @pubstate resets phase, history and trim.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool setInputRate(std::uint32_t inputRateHz) noexcept;

    /**
     * @brief    setTrimPpm — speed the conversion up (+) or down (−).
     *
     * @dname    setTrimPpm
     * @param    ppm  Clamped to ±kMaxTrimPpm.
     * @pubstate recomputes the phase step.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void setTrimPpm(std::int32_t ppm) noexcept;

    /**
     * @brief    inputRateHz — the rate set by setInputRate(), 0 if none.
     *
     * @dname    inputRateHz
     * @return   Input rate in Hz.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::uint32_t inputRateHz() const noexcept;

    /**
     * @brief    maxOutputFrames — output room needed for an input block.
     *
     * @dname    maxOutputFrames
     * @param    inputFrames  Frames that will be passed to process().
     * @return   Upper bound on frames process() can produce.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::size_t maxOutputFrames(std::size_t inputFrames) const
        noexcept;

    /**
     * @brief    process — convert one block; all input is consumed.
     *
     * @dname    process
     * @param    input   Interleaved L/R frames at the input rate.
     * @param    output  Interleaved L/R room for the result.
     * @return   Frames written to output (stops early if output is full).
     * @pubstate advances phase and keeps the last three input frames.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::size_t process(std::span<const std::int16_t> input,
                                      std::span<std::int16_t> output) noexcept;

private:
    static constexpr std::size_t kHistory = 3U;

    [[nodiscard]] std::int16_t frameAt(std::span<const std::int16_t> input,
                                       std::size_t index,
                                       std::size_t channel) const noexcept;
    [[nodiscard]] std::int16_t interpolate(
        std::span<const std::int16_t> input, std::size_t index, float t,
        std::size_t channel) const noexcept;
    void keepHistory(std::span<const std::int16_t> input) noexcept;
    void updateStep() noexcept;

    std::uint32_t inputRateHz_{0U};
    std::int32_t trimPpm_{0};
    std::uint64_t step_{0U};  ///< Input frames per output frame, Q32.
    std::uint64_t phase_{1ULL << 32U}; ///< Position in history+input, Q32.
    std::array<std::int16_t, kHistory * 2U> history_{};
};

/**
 * @brief    driftTrimPpm — trim that steers a playback buffer to half full.
 *
 * A fuller buffer means the producer runs faster than the 48 kHz clock,
 * so consume input slightly faster (positive trim), and vice versa.
 *
 * @dname    driftTrimPpm
 * @param    fillPercent  Current buffer fill 0–100.
 * @return   Trim in ppm, proportional and clamped to ±300.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::int32_t driftTrimPpm(unsigned fillPercent) noexcept;

} // namespace core
