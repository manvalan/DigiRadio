/**
 * @file    PcmResampler.cpp
 * @brief   Stereo 16-bit PCM sample-rate converter (cubic Hermite).
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

#include <algorithm>

namespace core {

namespace {

constexpr std::uint32_t kMinRateHz = 8000U;
constexpr std::uint32_t kMaxRateHz = 96000U;
constexpr std::uint64_t kOne = 1ULL << 32U;
constexpr std::int32_t kMaxDriftPpm = 300;
/** ppm of trim per percent of fill away from half: 50 % off -> 300 ppm. */
constexpr std::int32_t kDriftGain = 6;

[[nodiscard]] std::int16_t clampSample(float value) noexcept
{
    const float clamped = std::clamp(value, -32768.0F, 32767.0F);
    return static_cast<std::int16_t>(clamped);
}

} // namespace

bool PcmResampler::setInputRate(std::uint32_t inputRateHz) noexcept
{
    if (inputRateHz < kMinRateHz || inputRateHz > kMaxRateHz) {
        return false;
    }
    inputRateHz_ = inputRateHz;
    trimPpm_ = 0;
    phase_ = kOne;
    history_.fill(0);
    updateStep();
    return true;
}

void PcmResampler::setTrimPpm(std::int32_t ppm) noexcept
{
    trimPpm_ = std::clamp(ppm, -kMaxTrimPpm, kMaxTrimPpm);
    updateStep();
}

std::uint32_t PcmResampler::inputRateHz() const noexcept
{
    return inputRateHz_;
}

std::size_t PcmResampler::maxOutputFrames(std::size_t inputFrames) const
    noexcept
{
    if (step_ == 0U) {
        return 0U;
    }
    const std::uint64_t span = (static_cast<std::uint64_t>(inputFrames) + 1U)
                               << 32U;
    return static_cast<std::size_t>(span / step_) + 2U;
}

std::size_t PcmResampler::process(std::span<const std::int16_t> input,
                                  std::span<std::int16_t> output) noexcept
{
    const std::size_t frames = input.size() / 2U;
    const std::size_t capacity = output.size() / 2U;
    std::size_t written = 0U;
    while (step_ != 0U && (phase_ >> 32U) <= frames && written < capacity) {
        const auto index = static_cast<std::size_t>(phase_ >> 32U);
        const float t = static_cast<float>(phase_ & 0xFFFFFFFFULL)
                        / static_cast<float>(kOne);
        output[written * 2U] = interpolate(input, index, t, 0U);
        output[written * 2U + 1U] = interpolate(input, index, t, 1U);
        ++written;
        phase_ += step_;
    }
    const std::uint64_t consumed = static_cast<std::uint64_t>(frames) << 32U;
    phase_ = phase_ >= consumed + kOne ? phase_ - consumed : kOne;
    keepHistory(input);
    return written;
}

std::int16_t PcmResampler::frameAt(std::span<const std::int16_t> input,
                                   std::size_t index,
                                   std::size_t channel) const noexcept
{
    if (index < kHistory) {
        return history_[index * 2U + channel];
    }
    return input[(index - kHistory) * 2U + channel];
}

std::int16_t PcmResampler::interpolate(std::span<const std::int16_t> input,
                                       std::size_t index, float t,
                                       std::size_t channel) const noexcept
{
    const float xm1 = frameAt(input, index - 1U, channel);
    const float x0 = frameAt(input, index, channel);
    const float x1 = frameAt(input, index + 1U, channel);
    const float x2 = frameAt(input, index + 2U, channel);
    const float c1 = 0.5F * (x1 - xm1);
    const float c2 = xm1 - 2.5F * x0 + 2.0F * x1 - 0.5F * x2;
    const float c3 = 0.5F * (x2 - xm1) + 1.5F * (x0 - x1);
    return clampSample(((c3 * t + c2) * t + c1) * t + x0);
}

void PcmResampler::keepHistory(std::span<const std::int16_t> input) noexcept
{
    const std::size_t frames = input.size() / 2U;
    std::array<std::int16_t, kHistory * 2U> next{};
    for (std::size_t i = 0U; i < kHistory; ++i) {
        const std::size_t index = frames + i; // last three of history+input
        next[i * 2U] = frameAt(input, index, 0U);
        next[i * 2U + 1U] = frameAt(input, index, 1U);
    }
    history_ = next;
}

void PcmResampler::updateStep() noexcept
{
    if (inputRateHz_ == 0U) {
        step_ = 0U;
        return;
    }
    const double ratio = static_cast<double>(inputRateHz_)
                         / static_cast<double>(kOutputRateHz)
                         * (1.0 + static_cast<double>(trimPpm_) * 1e-6);
    step_ = static_cast<std::uint64_t>(ratio * static_cast<double>(kOne));
}

std::int32_t driftTrimPpm(unsigned fillPercent) noexcept
{
    const std::int32_t error = static_cast<std::int32_t>(
                                   std::min(fillPercent, 100U))
                               - 50;
    return std::clamp(error * kDriftGain, -kMaxDriftPpm, kMaxDriftPpm);
}

} // namespace core
