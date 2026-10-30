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
/** Time for a DAB ensemble to acquire and publish its service list. */
constexpr std::uint32_t kDabAcquireMs = 2500U;
/** Full status (RDS / DLS drain) once every this many ticks (~30 s). */
constexpr std::uint32_t kMetadataEveryTicks = 10U;

} // namespace

AfFollowService::AfFollowService(TunerService& tuner, SleepFn sleep)
    : tuner_(tuner)
    , sleep_(sleep)
{
}

void AfFollowService::tick(std::uint32_t nowMs)
{
    const std::lock_guard<std::recursive_mutex> lock(tuner_.mutex());
    if (!tuner_.afFollowStats().enabled || tuner_.sweepActive()) {
        return;
    }
    // Signal metrics every tick; RDS (AF list, PI) only every
    // kMetadataEveryTicks, so the background task stays light.
    const bool metadata = ticks_++ % kMetadataEveryTicks == 0U;
    auto status = metadata ? tuner_.refreshStatus() : tuner_.refreshSignal();
    if (!status) {
        return;
    }
    if (status->band == core::TunerBand::Fm) {
        if (!tickDabReturn(*status, nowMs)) {
            tickFm(*status, nowMs);
        }
    } else {
        tickDab(*status, nowMs);
    }
}

void AfFollowService::tickFm(const core::TunerStatus& status,
                             std::uint32_t nowMs)
{
    follower_.observe(status, nowMs);
    if (follower_.shouldHunt(nowMs)) {
        hunt(nowMs);
    }
}

void AfFollowService::tickDab(const core::TunerStatus& status,
                              std::uint32_t nowMs)
{
    dabFollower_.observe(status, nowMs);
    const auto sid = dabFollower_.serviceId();
    if (sid && dabFollower_.needsLinkage(nowMs)) {
        if (auto linkage = tuner_.readDabLinkage(*sid); linkage) {
            dabFollower_.setLinkage(std::move(*linkage), nowMs);
        }
    }
    if (dabFollower_.shouldHunt(nowMs)) {
        huntDab(status, nowMs);
    }
}

void AfFollowService::huntDab(const core::TunerStatus& status,
                              std::uint32_t nowMs)
{
    const std::uint32_t sid = *dabFollower_.serviceId();
    ESP_LOGI(kTag, "DAB service 0x%08x lost: trying linked sources",
             static_cast<unsigned>(sid));
    bool switched = false;
    for (const auto& candidate : dabFollower_.candidates()) {
        if (tryCandidate(candidate, sid)) {
            switched = true;
            if (candidate.fm) {
                armDabReturn(status, candidate, nowMs);
            }
            break;
        }
    }
    if (!switched) {
        restoreDab(status);
    }
    dabFollower_.finishHunt(nowMs);
    tuner_.recordDabFollow(switched);
}

bool AfFollowService::tryCandidate(const core::DabFollowCandidate& candidate,
                                   std::uint32_t serviceId)
{
    if (candidate.fm) {
        return tryFm(*candidate.fmFrequency, candidate.pi);
    }
    return tryEnsemble(candidate.dabFreqIndex, serviceId);
}

void AfFollowService::armDabReturn(const core::TunerStatus& status,
                                   const core::DabFollowCandidate& candidate,
                                   std::uint32_t nowMs)
{
    if (!status.dabFreqIndex || !status.dabPlayingServiceId
        || !status.dabPlayingComponentId) {
        return;
    }
    dabReturn_.arm(core::DabReturnTarget{
                       .freqIndex = *status.dabFreqIndex,
                       .serviceId = *status.dabPlayingServiceId,
                       .componentId = *status.dabPlayingComponentId,
                       .pi = candidate.pi},
                   nowMs);
    tuner_.setOnFmFallback(true);
}

// On the FM fallback: every dab_return_min minutes, check the original
// ensemble and go back if DAB is clearly good again. Returns true when
// this tick retuned (the FM follower must not read that snapshot).
bool AfFollowService::tickDabReturn(const core::TunerStatus& status,
                                    std::uint32_t nowMs)
{
    if (!dabReturn_.target()) {
        return false;
    }
    if (dabReturn_.isStale(status)) {
        dabReturn_.cancel();
        tuner_.setOnFmFallback(false);
        return false;
    }
    dabReturn_.setIntervalMinutes(tuner_.afFollowStats().dabReturnMinutes);
    if (!dabReturn_.shouldTry(status, nowMs) || !status.fmFrequency) {
        return false;
    }
    dabReturn_.recordAttempt(nowMs);
    if (returnToDab(*dabReturn_.target())) {
        dabReturn_.cancel();
        tuner_.setOnFmFallback(false);
        tuner_.recordDabReturn();
        return true;
    }
    if (!tuner_.tuneFm(*status.fmFrequency)) {
        ESP_LOGW(kTag, "could not retune FM %u kHz after DAB check",
                 static_cast<unsigned>(status.fmFrequency->value()));
    }
    return true;
}

bool AfFollowService::returnToDab(const core::DabReturnTarget& target)
{
    if (!tuner_.tuneDab(target.freqIndex)) {
        return false;
    }
    sleep_(kDabAcquireMs);
    const auto signal = tuner_.refreshSignal();
    if (!signal || !core::DabReturnPolicy::dabGoodEnough(*signal)) {
        return false;
    }
    const bool played =
        tuner_.playDabService(target.serviceId, target.componentId)
            .has_value();
    ESP_LOGI(kTag, "DAB service 0x%08x %s", static_cast<unsigned>(target.serviceId),
             played ? "back from FM" : "good but did not start");
    return played;
}

bool AfFollowService::tryEnsemble(std::uint8_t freqIndex,
                                  std::uint32_t serviceId)
{
    if (auto tuned = tuner_.tuneDab(freqIndex); !tuned) {
        return false;
    }
    sleep_(kDabAcquireMs);
    auto services = tuner_.listDabServices();
    if (!services) {
        return false;
    }
    for (const auto& entry : *services) {
        if (entry.serviceId == serviceId
            && tuner_.playDabService(entry.serviceId, entry.componentId)) {
            ESP_LOGI(kTag, "DAB service found on ensemble %u",
                     static_cast<unsigned>(freqIndex));
            return true;
        }
    }
    return false;
}

bool AfFollowService::tryFm(core::FrequencyKHz frequency, std::uint16_t pi)
{
    const auto sample = measure(frequency);
    const bool ok = sample && sample->locked
                    && sample->rssiDbuV >= core::AfFollowConfig{}.minRssiDbuV
                    && (!sample->pi || *sample->pi == pi);
    if (ok) {
        ESP_LOGI(kTag, "DAB service continued on FM %u kHz",
                 static_cast<unsigned>(frequency.value()));
    }
    return ok;
}

void AfFollowService::restoreDab(const core::TunerStatus& status)
{
    if (!status.dabFreqIndex || !status.dabPlayingServiceId
        || !status.dabPlayingComponentId) {
        return;
    }
    if (auto tuned = tuner_.tuneDab(*status.dabFreqIndex); tuned) {
        sleep_(kDabAcquireMs);
        (void)tuner_.playDabService(*status.dabPlayingServiceId,
                                    *status.dabPlayingComponentId);
    }
}

void AfFollowService::hunt(std::uint32_t nowMs)
{
    const auto current = follower_.currentSample();
    if (!current) {
        return;
    }
    tuner_.recordAfHunt();
    ESP_LOGI(kTag, "weak signal (%d dBuV, locked %d): probing AFs",
             static_cast<int>(current->rssiDbuV),
             static_cast<int>(current->locked));
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
