/**
 * @file    IntegrationService.hpp
 * @brief   Orchestrates tuner, audio, and preset recall for app_main.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "audio/AudioService.hpp"
#include "core/IntegrationError.hpp"
#include "core/ISecureStore.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"

#include <cstddef>
#include <expected>

namespace integration {

/**
 * @brief    IntegrationService — end-to-end radio listening orchestration.
 *
 * @dname    IntegrationService
 * @return   n/a (type)
 * @pubstate Borrows store, tuner, audio, and stations for process lifetime.
 *           Called from app_main for startup and from HTTP for preset recall.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class IntegrationService {
public:
    /**
     * @brief    IntegrationService — bind application services.
     *
     * @dname    IntegrationService
     * @param    store     Secure persistence for presets and last recall index.
     * @param    tuner     Tuner orchestration.
     * @param    audio     ADAU1701 profile orchestration.
     * @param    stations  Preset list CRUD and tune delegation.
     * @pubstate stores references; no side effects until startup().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    IntegrationService(core::ISecureStore& store,
                       tuner::TunerService& tuner,
                       audio::AudioService& audio,
                       station::StationService& stations);

    /**
     * @brief    startup — load presets and recall the last station if stored.
     *
     * @dname    startup
     * @return   Ok on success, or IntegrationError when recall fails.
     * @pubstate loads station list; may tune and refresh the audio profile.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::IntegrationError> startup();

    /**
     * @brief    recallPreset — tune a preset and apply the saved audio profile.
     *
     * @dname    recallPreset
     * @param    index  Zero-based preset list position.
     * @return   Ok on success, or IntegrationError.
     * @pubstate tunes via StationService, safeloads AudioProfile, persists index.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::IntegrationError> recallPreset(
        std::size_t index);

    /**
     * @brief    stations — access the preset list service.
     *
     * @dname    stations
     * @return   Reference to the injected StationService.
     * @pubstate reads stations_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] station::StationService& stations() noexcept;

    /**
     * @brief    tuner — access the tuner service.
     *
     * @dname    tuner
     * @return   Reference to the injected TunerService.
     * @pubstate reads tuner_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] tuner::TunerService& tuner() noexcept;

    /**
     * @brief    audio — access the audio service.
     *
     * @dname    audio
     * @return   Reference to the injected AudioService.
     * @pubstate reads audio_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] audio::AudioService& audio() noexcept;

private:
    [[nodiscard]] std::expected<void, core::IntegrationError>
    applyStoredAudioProfile();

    [[nodiscard]] std::expected<void, core::StoreError> persistLastPreset(
        std::size_t index);

    core::ISecureStore& store_;
    tuner::TunerService& tuner_;
    audio::AudioService& audio_;
    station::StationService& stations_;
};

} // namespace integration
