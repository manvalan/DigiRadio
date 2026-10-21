/**
 * @file    AlternateFrequencyFollower.cpp
 * @brief   AlternateFrequencyFollower implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/AlternateFrequencyFollower.hpp"

#include <algorithm>

namespace core {

namespace {

constexpr std::int8_t kUnknownRssi = -128;

[[nodiscard]] std::optional<AfSample> sampleFrom(const TunerStatus& status)
{
    if (status.band != TunerBand::Fm || !status.fmFrequency) {
        return std::nullopt;
    }
    return AfSample{*status.fmFrequency,
                    status.fmRssiDbuV.value_or(kUnknownRssi),
                    status.locked,
                    status.fmProgramId};
}

} // namespace

AlternateFrequencyFollower::AlternateFrequencyFollower(AfFollowConfig config)
    : config_(config)
{
}

void AlternateFrequencyFollower::observe(const TunerStatus& status,
                                         std::uint32_t nowMs)
{
    const auto sample = sampleFrom(status);
    if (!sample) {
        return;
    }
    const std::uint32_t khz = sample->frequency.value();
    const bool piChanged = stationPi_ && sample->pi && *sample->pi != *stationPi_;
    if (!current_ || !inStation(khz) || piChanged) {
        startStation(*sample, nowMs);
    }
    current_ = sample;
    const bool weak = !sample->locked || sample->rssiDbuV < config_.weakRssiDbuV;
    weakStreak_ = weak ? static_cast<std::uint8_t>(weakStreak_ + (weakStreak_ < 255U ? 1U : 0U))
                       : 0U;
    mergeStatus(status);
}

bool AlternateFrequencyFollower::shouldHunt(std::uint32_t nowMs) const
{
    if (!current_ || afSet_.size() < 2U) {
        return false;
    }
    if (nowMs - stationSinceMs_ < config_.settleMs) {
        return false;
    }
    if (lastHuntMs_ && nowMs - *lastHuntMs_ < config_.cooldownMs) {
        return false;
    }
    return weakStreak_ >= config_.weakObservations;
}

std::vector<FrequencyKHz> AlternateFrequencyFollower::candidates() const
{
    std::vector<FrequencyKHz> out;
    const std::uint32_t currentKhz = current_ ? current_->frequency.value() : 0U;
    for (const std::uint32_t khz : afSet_) {
        if (out.size() >= config_.maxCandidates) {
            break;
        }
        if (khz == currentKhz) {
            continue;
        }
        if (auto freq = FrequencyKHz::tryFromKhz(khz); freq) {
            out.push_back(*freq);
        }
    }
    return out;
}

std::optional<AfSample> AlternateFrequencyFollower::currentSample() const
{
    return current_;
}

FrequencyKHz AlternateFrequencyFollower::choose(
    const AfSample& current, std::span<const AfSample> probed) const
{
    const AfSample* best = nullptr;
    for (const AfSample& sample : probed) {
        if (samePi(sample) && (best == nullptr || better(sample, *best))) {
            best = &sample;
        }
    }
    if (best == nullptr || !usable(*best)) {
        return current.frequency;
    }
    const int gain = static_cast<int>(best->rssiDbuV) - current.rssiDbuV;
    if (gain >= config_.improvementDb || !usable(current)) {
        return best->frequency;
    }
    return current.frequency;
}

void AlternateFrequencyFollower::finishHunt(FrequencyKHz chosen,
                                            std::uint32_t nowMs)
{
    lastHuntMs_ = nowMs;
    weakStreak_ = 0U;
    addFrequency(chosen.value());
}

bool AlternateFrequencyFollower::inStation(std::uint32_t khz) const
{
    return std::find(afSet_.begin(), afSet_.end(), khz) != afSet_.end();
}

bool AlternateFrequencyFollower::usable(const AfSample& sample) const
{
    return sample.locked && sample.rssiDbuV >= config_.minRssiDbuV;
}

bool AlternateFrequencyFollower::samePi(const AfSample& sample) const
{
    return !stationPi_ || !sample.pi || *sample.pi == *stationPi_;
}

bool AlternateFrequencyFollower::better(const AfSample& lhs,
                                        const AfSample& rhs) const
{
    if (lhs.locked != rhs.locked) {
        return lhs.locked;
    }
    return lhs.rssiDbuV > rhs.rssiDbuV;
}

void AlternateFrequencyFollower::startStation(const AfSample& sample,
                                              std::uint32_t nowMs)
{
    afSet_.clear();
    afSet_.push_back(sample.frequency.value());
    stationPi_ = sample.pi;
    stationSinceMs_ = nowMs;
    lastHuntMs_.reset();
    weakStreak_ = 0U;
}

void AlternateFrequencyFollower::addFrequency(std::uint32_t khz)
{
    if (!inStation(khz)) {
        afSet_.push_back(khz);
    }
}

void AlternateFrequencyFollower::mergeStatus(const TunerStatus& status)
{
    for (const FrequencyKHz& af : status.fmAlternateFrequencies) {
        addFrequency(af.value());
    }
    if (!stationPi_ && status.fmProgramId) {
        stationPi_ = status.fmProgramId;
    }
}

} // namespace core
