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

#include "esp_log.h"

namespace audio {

namespace {

constexpr char kTag[] = "AudioService";

} // namespace

AudioService::AudioService(core::IDsp& dsp, core::IAudioProfileStore* store)
    : dsp_(dsp)
    , store_(store)
    , profile_(core::AudioProfile::factoryDefault())
{
}

std::expected<bool, core::DspError> AudioService::loadAndApply()
{
    bool restored = false;
    if (store_ != nullptr && store_->hasProfile()) {
        if (auto loaded = store_->loadProfile(); loaded) {
            profile_ = *loaded;
            restored = true;
            ESP_LOGI(kTag, "audio profile loaded from NVS");
        } else {
            ESP_LOGW(kTag, "audio profile present but failed to load — "
                          "using factory default");
        }
    } else {
        ESP_LOGI(kTag, "no saved audio profile — using factory default");
    }

    if (auto applied = applyProfileToDsp(profile_); !applied) {
        return std::unexpected(core::DspError::SafeloadFailed);
    }
    return restored;
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
    auto saved = store_->saveProfile(profile_);
    if (!saved) {
        ESP_LOGW(kTag, "persistProfile: NVS save failed");
    }
    return saved;
}

std::expected<void, core::StoreError> AudioService::applyProfileToDsp(
    const core::AudioProfile& profile)
{
    if (auto applied = dsp_.applyProfile(profile); !applied) {
        ESP_LOGW(kTag, "applyProfileToDsp: DSP safeload failed");
        return std::unexpected(core::StoreError::IoFailed);
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

std::expected<void, core::StoreError> AudioService::selectSource(
    core::ActiveSource source, bool persist)
{
    if (auto applied = dsp_.selectSource(source); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.activeSource = source;
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

std::expected<void, core::StoreError> AudioService::applyRadioFirstMix(
    bool persist)
{
    const core::GainDb unity = core::GainDb::zero();
    profile_.activeSource = core::ActiveSource::Radio;
    profile_.masterLeft = unity;
    profile_.masterRight = unity;
    if (auto applied = dsp_.selectSource(profile_.activeSource); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    if (auto master = dsp_.setMasterVolume(unity, unity); !master) {
        return std::unexpected(core::StoreError::IoFailed);
    }
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
    if (auto applied = dsp_.applyEq(profile_.eq); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::setStereoEnhance(
    core::EnhanceLevel level, bool persist)
{
    if (auto applied = dsp_.setStereoSpreadLevel(level); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.enhancements.stereo = level;
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::setBassEnhance(
    core::EnhanceLevel level, bool persist)
{
    if (auto applied = dsp_.setBassBoostLevel(level); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.enhancements.bass = level;
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::DspError> AudioService::setBeepEnabled(bool enabled)
{
    return dsp_.setBeepEnabled(enabled);
}

std::expected<void, core::DspError> AudioService::writeRawParam(
    unsigned address, float value)
{
    return dsp_.writeRawParam(address, value);
}

std::expected<core::AudioLevels, core::DspError> AudioService::readLevels()
{
    return dsp_.readLevels();
}

} // namespace audio
