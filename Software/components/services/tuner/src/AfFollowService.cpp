/**
 * @file    AfFollowService.cpp
 * @brief   AfFollowService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "tuner/AfFollowService.hpp"

#include "esp_log.h"

#include <mutex>

namespace tuner {

namespace {

constexpr char kTag[] = "AfFollow";
/** Time for RSQ to settle after FM_TUNE_FREQ (matches the app's 250 ms). */
constexpr std::uint32_t kSettleMs = 250U;
/** Extra RDS reads so a PI can be confirmed (two matching groups). */
constexpr int kPiReads = 3;
constexpr std::uint32_t kPiReadGapMs = 120U;

} // namespace

AfFollowService::AfFollowService(TunerService& tuner, SleepFn sleep)
    : tuner_(tuner)
    , sleep_(sleep)
{
}

void AfFollowService::tick(std::uint32_t nowMs)
{
    const std::lock_guard<std::recursive_mutex> lock(tuner_.mutex());
    if (!tuner_.afFollowStats().enabled) {
        return;
    }
    auto status = tuner_.refreshStatus();
    if (!status || status->band != core::TunerBand::Fm) {
        return;
    }
    follower_.observe(*status, nowMs);
    if (follower_.shouldHunt(nowMs)) {
        hunt(nowMs);
    }
}

void AfFollowService::hunt(std::uint32_t nowMs)
{
    const auto current = follower_.currentSample();
    if (!current) {
        return;
    }
    const auto samples = probe(follower_.candidates());
    const core::FrequencyKHz chosen = follower_.choose(*current, samples);
    if (auto tuned = tuner_.tuneFm(chosen); !tuned) {
        ESP_LOGW(kTag, "retune to %u kHz failed",
                 static_cast<unsigned>(chosen.value()));
    }
    follower_.finishHunt(chosen, nowMs);
    if (chosen.value() != current->frequency.value()) {
        ESP_LOGI(kTag, "weak signal (%d dBuV): %u -> %u kHz",
                 static_cast<int>(current->rssiDbuV),
                 static_cast<unsigned>(current->frequency.value()),
                 static_cast<unsigned>(chosen.value()));
        tuner_.recordAfSwitch(current->frequency, chosen);
    }
}

std::vector<core::AfSample> AfFollowService::probe(
    const std::vector<core::FrequencyKHz>& candidates)
{
    std::vector<core::AfSample> samples;
    for (const core::FrequencyKHz& frequency : candidates) {
        if (auto sample = measure(frequency); sample) {
            samples.push_back(*sample);
        }
    }
    return samples;
}

std::optional<core::AfSample> AfFollowService::measure(
    core::FrequencyKHz frequency)
{
    if (auto tuned = tuner_.tuneFm(frequency); !tuned) {
        return std::nullopt;
    }
    sleep_(kSettleMs);
    auto status = tuner_.refreshStatus();
    for (int i = 0; i < kPiReads && status && !status->fmProgramId; ++i) {
        sleep_(kPiReadGapMs);
        status = tuner_.refreshStatus();
    }
    if (!status) {
        return std::nullopt;
    }
    return core::AfSample{frequency,
                          status->fmRssiDbuV.value_or(-128),
                          status->locked,
                          status->fmProgramId};
}

} // namespace tuner
