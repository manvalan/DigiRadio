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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>

namespace audio {

namespace {

constexpr char kTag[] = "AudioService";

// Standby mute floor: GainDb's lowest legal value. -96 dB in the ADAU1701
// 5.23 gain format is ~132 LSB of 2^23 (core::gainDbToLinearFixpoint), far
// below audibility, so no new DSP cell or register is needed.
[[nodiscard]] core::GainDb muteGain()
{
    return *core::GainDb::tryFromDb(core::GainDb::kMinDb);
}

/** Attenuation steps of the source-switch fade (dB below the user level). */
constexpr std::array<float, 3> kFadeStepsDb{-8.0F, -20.0F, -40.0F};

[[nodiscard]] core::GainDb attenuated(core::GainDb level, float stepDb)
{
    const float db = level.value() + stepDb;
    return *core::GainDb::tryFromDb(db < core::GainDb::kMinDb
                                        ? core::GainDb::kMinDb
                                        : db);
}

} // namespace

AudioService::AudioService(core::IDsp& dsp, core::IAudioProfileStore* store)
    : dsp_(dsp)
    , store_(store)
    , profile_(core::AudioProfile::factoryDefault())
    , powerState_(core::PowerState::On)
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
    core::AudioProfile output = profile;
    if (powerState_ == core::PowerState::Standby) {
        output.masterLeft = muteGain();
        output.masterRight = muteGain();
    }
    if (auto applied = dsp_.applyProfile(output); !applied) {
        ESP_LOGW(kTag, "applyProfileToDsp: DSP safeload failed");
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::applyProfile(
    const core::AudioProfile& profile, bool persist)
{
    if (profile.activeSource == profile_.activeSource) {
        if (auto applied = applyProfileToDsp(profile); !applied) {
            return applied;
        }
        profile_ = profile;
    } else if (auto switched = applySwitchingProfile(profile); !switched) {
        return switched;
    }
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::selectSource(
    core::ActiveSource source, bool persist)
{
    if (auto applied = switchInput(source); !applied) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    if (persist) {
        return persistProfile();
    }
    return {};
}

std::expected<void, core::StoreError> AudioService::applySwitchingProfile(
    const core::AudioProfile& profile)
{
    fadeMaster(true);
    core::AudioProfile muted = profile;
    muted.masterLeft = muteGain();
    muted.masterRight = muteGain();
    auto applied = applyProfileToDsp(muted);
    if (applied) {
        profile_ = profile;
    }
    fadeMaster(false);
    return applied;
}

std::expected<void, core::StoreError> AudioService::applyPreset(
    const core::AudioPreset& preset, bool persist)
{
    const bool ok = dsp_.applyEq(preset.eq)
                    && dsp_.setBassBoostLevel(preset.enhancements.bass)
                    && dsp_.setStereoSpreadLevel(preset.enhancements.stereo);
    if (!ok) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    profile_.eq = preset.eq;
    profile_.enhancements = preset.enhancements;
    if (persist) {
        return persistProfile();
    }
    return {};
}

void AudioService::setSourceListener(
    core::IAudioSourceListener* listener) noexcept
{
    listener_ = listener;
}

std::expected<void, core::DspError> AudioService::switchInput(
    core::ActiveSource source)
{
    if (source == profile_.activeSource) {
        return dsp_.selectSource(source); // re-assert, nothing to fade
    }
    fadeMaster(true);
    auto applied = dsp_.selectSource(source);
    if (applied) {
        profile_.activeSource = source;
        if (listener_ != nullptr) {
            listener_->onSourceSelected(source);
        }
    }
    fadeMaster(false);
    return applied;
}

void AudioService::fadeMaster(bool down)
{
    if (powerState_ == core::PowerState::Standby) {
        return; // already muted
    }
    for (std::size_t i = 0; i < kFadeStepsDb.size(); ++i) {
        const float step =
            kFadeStepsDb[down ? i : kFadeStepsDb.size() - 1U - i];
        // Best effort: a failed step only makes the switch less smooth.
        (void)dsp_.setMasterVolume(attenuated(profile_.masterLeft, step),
                                   attenuated(profile_.masterRight, step));
        vTaskDelay(1);
    }
    if (!down) {
        (void)writeMaster(profile_.masterLeft, profile_.masterRight);
    }
}

std::expected<void, core::StoreError> AudioService::setMasterVolume(
    core::GainDb left, core::GainDb right, bool persist)
{
    if (auto applied = writeMaster(left, right); !applied) {
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
    if (auto master = writeMaster(unity, unity); !master) {
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

std::expected<void, core::DspError> AudioService::writeMaster(
    core::GainDb left, core::GainDb right)
{
    if (powerState_ == core::PowerState::Standby) {
        return dsp_.setMasterVolume(muteGain(), muteGain());
    }
    return dsp_.setMasterVolume(left, right);
}

std::expected<void, core::DspError> AudioService::setPowerState(
    core::PowerState state)
{
    const core::PowerState previous = powerState_;
    powerState_ = state;
    auto applied = writeMaster(profile_.masterLeft, profile_.masterRight);
    if (!applied) {
        powerState_ = previous;
        ESP_LOGW(kTag, "setPowerState: master safeload failed");
        return applied;
    }
    ESP_LOGI(kTag, "power state: %s",
             state == core::PowerState::Standby ? "standby" : "on");
    return {};
}

core::PowerState AudioService::powerState() const noexcept
{
    return powerState_;
}

std::expected<core::AudioLevels, core::DspError> AudioService::readLevels()
{
    return dsp_.readLevels();
}

} // namespace audio
