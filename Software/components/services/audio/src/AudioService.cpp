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

    if (auto applied = dsp_.applyProfile(profile_); !applied) {
        return applied;
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

std::expected<void, core::StoreError> AudioService::applyProfile(
    const core::AudioProfile& profile, bool persist)
{
    if (auto applied = dsp_.applyProfile(profile); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
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
    if (auto applied = dsp_.setEqBand(band, gain, center, q); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.eq.setBand(band, core::EqBandSettings{
                                 .gain = gain,
                                 .center = center,
                                 .q = q,
                             });
    if (persist) {
        return persistProfile();
    }
    return {};
}

} // namespace audio
