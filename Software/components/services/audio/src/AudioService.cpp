/**
 * @file    AudioService.cpp
 * @brief   AudioService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "audio/AudioService.hpp"

namespace audio {

namespace {

[[nodiscard]] core::AudioProfile profileForHardware(
    const core::AudioProfile& profile) noexcept
{
    core::AudioProfile hardware = profile;
    hardware.eq =
        core::applyEnhancementsToEq(profile.eq, profile.enhancements);
    return hardware;
}

} // namespace

AudioService::AudioService(core::IDsp& dsp, core::IAudioProfileStore* store)
    : dsp_(dsp)
    , store_(store)
    , profile_(core::AudioProfile::factoryDefault())
{
}

std::expected<void, core::DspError> AudioService::loadAndApply()
{
    if (store_ != nullptr && store_->hasProfile()) {
        if (auto loaded = store_->loadProfile(); loaded) {
            profile_ = *loaded;
        }
    }

    if (auto applied = applyProfileToDsp(profile_); !applied) {
        return std::unexpected(core::DspError::SafeloadFailed);
    }
    return {};
}

const core::AudioProfile& AudioService::currentProfile() const noexcept
{
    return profile_;
}

std::expected<void, core::StoreError> AudioService::persistProfile() const
{
    if (store_ == nullptr) {
        return {};
    }
    return store_->saveProfile(profile_);
}

std::expected<void, core::StoreError> AudioService::applyProfileToDsp(
    const core::AudioProfile& profile)
{
    const core::AudioProfile hardware = profileForHardware(profile);
    if (auto applied = dsp_.applyProfile(hardware); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::applyEffectiveEq(
    bool persist)
{
    const core::EqProfile effective = core::applyEnhancementsToEq(
        profile_.eq, profile_.enhancements);
    if (auto applied = dsp_.applyEq(effective); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::applyProfile(
    const core::AudioProfile& profile, bool persist)
{
    if (auto applied = applyProfileToDsp(profile); !applied) {
        return applied;
    }
    profile_ = profile;
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::setInputVolume(
    core::MixSource source, core::GainDb left, core::GainDb right, bool persist)
{
    if (auto applied = dsp_.setInputVolume(source, left, right); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    if (source == core::MixSource::Si4684) {
        profile_.mixer.si4684Left = left;
        profile_.mixer.si4684Right = right;
    } else {
        profile_.mixer.esp32Left = left;
        profile_.mixer.esp32Right = right;
    }

    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::setMasterVolume(
    core::GainDb left, core::GainDb right, bool persist)
{
    if (auto applied = dsp_.setMasterVolume(left, right); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.masterLeft = left;
    profile_.masterRight = right;
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::setEqBand(
    core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center, float q,
    bool persist)
{
    profile_.eq.setBand(band, core::EqBandSettings{
                                 .gain = gain,
                                 .center = center,
                                 .q = q,
                             });
    return applyEffectiveEq(persist);
}

std::expected<void, core::StoreError> AudioService::setStereoEnhance(
    core::EnhanceLevel level, bool persist)
{
    profile_.enhancements.stereo = level;
    return applyEffectiveEq(persist);
}

std::expected<void, core::StoreError> AudioService::setBassEnhance(
    core::EnhanceLevel level, bool persist)
{
    profile_.enhancements.bass = level;
    return applyEffectiveEq(persist);
}

} // namespace audio
