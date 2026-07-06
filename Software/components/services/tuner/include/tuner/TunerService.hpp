/**
 * @file    TunerService.hpp
 * @brief   Application service orchestrating tuner operations for UI/API.
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

#include "core/FrequencyKHz.hpp"
#include "core/ITuner.hpp"
#include "core/SeekDirection.hpp"
#include "core/TunerError.hpp"
#include "core/TunerStatus.hpp"

#include <cstdint>
#include <expected>
#include <vector>

namespace tuner {

/**
 * @brief    TunerService — intent-level tuner API for HTTP and future UI.
 *
 * @dname    TunerService
 * @return   n/a (type)
 * @pubstate Borrows core::ITuner for the process lifetime. Tracks last tune
 *           target and cached volume for status reporting. No public data
 *           members.
 *
 * Delegates hardware to core::ITuner; maps driver failures to TunerError
 * without exposing SPI details to the shell.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class TunerService {
public:
    /**
     * @brief    TunerService — bind to a tuner driver for the process lifetime.
     *
     * @dname    TunerService
     * @param    tuner  Driver implementation (must outlive this service).
     * @pubstate stores tuner reference; initialises last tune defaults.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit TunerService(core::ITuner& tuner);

    /**
     * @brief    refreshStatus — read a fresh tuner snapshot from the driver.
     *
     * @dname    refreshStatus
     * @return   TunerStatus on success, or a TunerError from ITuner.
     * @pubstate reads tuner_; updates cached volume_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError>
    refreshStatus();

    /**
     * @brief    tuneDab — tune to a Band III ensemble index.
     *
     * @dname    tuneDab
     * @param    freqIndex  Ensemble index 0–37.
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate writes lastDabIndex_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneDab(
        std::uint8_t freqIndex);

    /**
     * @brief    tuneFm — tune to an FM centre frequency.
     *
     * @dname    tuneFm
     * @param    frequency  Validated FM centre frequency.
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate writes lastFmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneFm(
        core::FrequencyKHz frequency);

    /**
     * @brief    seekFm — seek FM in the given direction.
     *
     * @dname    seekFm
     * @param    direction  Up or Down scan direction.
     * @return   New centre frequency, or a TunerError.
     * @pubstate writes lastFmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::FrequencyKHz, core::TunerError> seekFm(
        core::SeekDirection direction);

    /**
     * @brief    listDabServices — programmes available on the current ensemble.
     *
     * @dname    listDabServices
     * @return   Service entries, or a TunerError from ITuner.
     * @pubstate reads tuner_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<std::vector<core::TunerServiceEntry>,
                                core::TunerError>
    listDabServices();

    /**
     * @brief    playDabService — start playback of a DAB programme.
     *
     * @dname    playDabService
     * @param    serviceId    Selected service identifier.
     * @param    componentId  Audio component within the service.
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate delegates to tuner_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> playDabService(
        std::uint32_t serviceId, std::uint32_t componentId);

    /**
     * @brief    setVolume — set tuner output attenuation.
     *
     * @dname    setVolume
     * @param    level  Attenuator 0–63.
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate writes volume_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> setVolume(
        std::uint8_t level);

    /**
     * @brief    tuner — borrow the underlying driver for diagnostics.
     *
     * @dname    tuner
     * @return   Reference to the injected ITuner implementation.
     * @pubstate reads tuner_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] core::ITuner& tuner() noexcept;

private:
    core::ITuner& tuner_;
    std::uint8_t lastDabIndex_;
    core::FrequencyKHz lastFmFrequency_;
    std::uint8_t volume_;
};

} // namespace tuner
