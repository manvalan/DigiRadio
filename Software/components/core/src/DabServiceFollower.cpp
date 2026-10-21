/**
 * @file    DabServiceFollower.cpp
 * @brief   DabServiceFollower implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */

#include "core/DabServiceFollower.hpp"

#include <algorithm>

namespace core {

namespace {

template <typename T>
[[nodiscard]] bool contains(const std::vector<T>& values, T value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

[[nodiscard]] bool isDabEnsemble(const DabFrequencyInfo& fi)
{
    return fi.rnm == kFiRnmDabEnsemble || fi.rnm == kFiRnmDabEnsembleLocal;
}

[[nodiscard]] bool sameCandidate(const DabFollowCandidate& a,
                                 const DabFollowCandidate& b)
{
    if (a.fm != b.fm) {
        return false;
    }
    return a.fm ? a.fmFrequency->value() == b.fmFrequency->value()
                : a.dabFreqIndex == b.dabFreqIndex;
}

void pushUnique(std::vector<DabFollowCandidate>& out, DabFollowCandidate c)
{
    const auto same = [&c](const DabFollowCandidate& o) {
        return sameCandidate(o, c);
    };
    if (std::none_of(out.begin(), out.end(), same)) {
        out.push_back(c);
    }
}

} // namespace

DabServiceFollower::DabServiceFollower(DabFollowConfig config)
    : config_(config)
{
}

void DabServiceFollower::observe(const TunerStatus& status, std::uint32_t nowMs)
{
    if (status.band != TunerBand::Dab || !status.dabPlayingServiceId
        || !status.dabFreqIndex) {
        return;
    }
    if (serviceId_ != status.dabPlayingServiceId
        || freqIndex_ != status.dabFreqIndex) {
        serviceId_ = status.dabPlayingServiceId;
        freqIndex_ = status.dabFreqIndex;
        linkage_.reset();
        serviceSinceMs_ = nowMs;
        lastHuntMs_.reset();
        weakStreak_ = 0U;
    }
    weakStreak_ = weak(status)
                      ? static_cast<std::uint8_t>(std::min(weakStreak_ + 1, 255))
                      : 0U;
}

bool DabServiceFollower::needsLinkage(std::uint32_t nowMs) const
{
    if (!serviceId_ || weakStreak_ > 0U) {
        return false;
    }
    return !linkage_ || nowMs - linkageAtMs_ >= config_.linkageMaxAgeMs;
}

void DabServiceFollower::setLinkage(DabLinkage linkage, std::uint32_t nowMs)
{
    linkage_ = std::move(linkage);
    linkageAtMs_ = nowMs;
}

bool DabServiceFollower::shouldHunt(std::uint32_t nowMs) const
{
    if (!serviceId_ || weakStreak_ < config_.weakObservations) {
        return false;
    }
    if (nowMs - serviceSinceMs_ < config_.settleMs) {
        return false;
    }
    if (lastHuntMs_ && nowMs - *lastHuntMs_ < config_.cooldownMs) {
        return false;
    }
    return !candidates().empty();
}

std::vector<DabFollowCandidate> DabServiceFollower::candidates() const
{
    std::vector<DabFollowCandidate> out;
    if (!linkage_) {
        return out;
    }
    addDabCandidates(out);
    addFmCandidates(out);
    if (out.size() > config_.maxCandidates) {
        out.resize(config_.maxCandidates);
    }
    return out;
}

std::optional<std::uint32_t> DabServiceFollower::serviceId() const
{
    return serviceId_;
}

void DabServiceFollower::finishHunt(std::uint32_t nowMs)
{
    lastHuntMs_ = nowMs;
    weakStreak_ = 0U;
}

bool DabServiceFollower::weak(const TunerStatus& status) const
{
    return !status.locked
           || status.dabFicQuality.value_or(0U) < config_.weakFicQuality;
}

std::vector<std::uint16_t> DabServiceFollower::hardLinkedPis() const
{
    std::vector<std::uint16_t> pis;
    for (const DabLinkageSet& set : linkage_->sets) {
        if (set.type != DabLinkType::RdsPi || !set.hard) {
            continue;
        }
        for (const std::uint32_t id : set.ids) {
            pis.push_back(static_cast<std::uint16_t>(id & 0xFFFFU));
        }
    }
    return pis;
}

void DabServiceFollower::addDabCandidates(
    std::vector<DabFollowCandidate>& out) const
{
    for (const DabFrequencyInfo& fi : linkage_->frequencies) {
        const auto eid = static_cast<std::uint16_t>(fi.id & 0xFFFFU);
        if (!isDabEnsemble(fi) || fi.freqIndex == kFiNoFreqIndex
            || fi.freqIndex == freqIndex_ || !contains(linkage_->otherEnsembles, eid)) {
            continue;
        }
        pushUnique(out, DabFollowCandidate{false, fi.freqIndex, std::nullopt, 0U});
    }
}

void DabServiceFollower::addFmCandidates(
    std::vector<DabFollowCandidate>& out) const
{
    const std::vector<std::uint16_t> pis = hardLinkedPis();
    for (const DabFrequencyInfo& fi : linkage_->frequencies) {
        const auto pi = static_cast<std::uint16_t>(fi.id & 0xFFFFU);
        if (fi.rnm != kFiRnmFmRds || !contains(pis, pi)) {
            continue;
        }
        if (auto khz = FrequencyKHz::tryFromKhz(fi.frequencyHz / 1000U); khz) {
            pushUnique(out, DabFollowCandidate{true, 0U, *khz, pi});
        }
    }
}

} // namespace core
