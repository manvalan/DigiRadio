/**
 * @file    IntegrationService.cpp
 * @brief   IntegrationService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "integration/IntegrationService.hpp"

namespace integration {

IntegrationService::IntegrationService(core::ISecureStore& store,
                                       tuner::TunerService& tuner,
                                       audio::AudioService& audio,
                                       station::StationService& stations)
    : store_(store)
    , tuner_(tuner)
    , audio_(audio)
    , stations_(stations)
{
}

std::expected<void, core::IntegrationError> IntegrationService::startup()
{
    if (auto loaded = stations_.loadFromStore(); !loaded) {
        return std::unexpected(core::IntegrationError::StoreFailed);
    }

    if (!store_.hasLastPresetIndex()) {
        return {};
    }

    const auto index = store_.loadLastPresetIndex();
    if (!index) {
        return std::unexpected(core::IntegrationError::StoreFailed);
    }

    if (*index >= stations_.list().stations().size()) {
        (void)store_.clearLastPresetIndex();
        return {};
    }

    (void)recallPreset(*index);
    return {};
}

std::expected<void, core::IntegrationError> IntegrationService::recallPreset(
    std::size_t index)
{
    if (index >= stations_.list().stations().size()) {
        return std::unexpected(core::IntegrationError::PresetNotFound);
    }

    if (auto tuned = stations_.tuneToIndex(index); !tuned) {
        return std::unexpected(core::IntegrationError::TuneFailed);
    }

    if (auto applied = applyStoredAudioProfile(); !applied) {
        return applied;
    }

    if (auto saved = persistLastPreset(index); !saved) {
        return std::unexpected(core::IntegrationError::StoreFailed);
    }

    return {};
}

station::StationService& IntegrationService::stations() noexcept
{
    return stations_;
}

tuner::TunerService& IntegrationService::tuner() noexcept
{
    return tuner_;
}

audio::AudioService& IntegrationService::audio() noexcept
{
    return audio_;
}

std::expected<void, core::IntegrationError>
IntegrationService::applyStoredAudioProfile()
{
    if (auto applied = audio_.applyProfile(audio_.currentProfile(), false);
        !applied) {
        return std::unexpected(core::IntegrationError::AudioFailed);
    }
    return {};
}

std::expected<void, core::StoreError> IntegrationService::persistLastPreset(
    std::size_t index)
{
    if (index > 255U) {
        return std::unexpected(core::StoreError::InvalidData);
    }
    return store_.saveLastPresetIndex(static_cast<std::uint8_t>(index));
}

} // namespace integration
