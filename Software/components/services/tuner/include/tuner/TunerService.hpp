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

#include "core/AlternateFrequencyFollower.hpp"
#include "core/FrequencyKHz.hpp"
#include "core/ITuner.hpp"
#include "core/SeekDirection.hpp"
#include "core/TunerError.hpp"
#include "core/TunerJson.hpp"
#include "core/TunerStatus.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <vector>

namespace tuner {

/**
 * @brief    TunerService — intent-level tuner API for HTTP and future UI.
 *
 * @dname    TunerService
 * @return   n/a (type)
 * @pubstate Borrows core::ITuner for the process lifetime. Tracks last tune
 *           target, cached volume and RDS AF-follow statistics. Every
 *           public tuner operation holds a recursive mutex, so the HTTP
 *           task and the AF-follow task never interleave SPI commands.
 *           No public data members.
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
     * @param    antCap     Front-end antenna varactor override for this one
     *                      tune (e.g. for a calibration sweep). Omit to use
     *                      defaultDabAntCap_ (the board's saved calibration,
     *                      or hardware auto-tune if never calibrated).
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate writes lastDabIndex_ on success; clears last-played DAB ids.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneDab(
        std::uint8_t freqIndex,
        std::optional<std::uint8_t> antCap = std::nullopt);

    /**
     * @brief    setDefaultDabAntCap — set the board's calibrated DAB ANTCAP.
     *
     * @dname    setDefaultDabAntCap
     * @param    antCap  Value applied to every DAB tune that doesn't pass an
     *                   explicit override (0 = chip auto-tune, the factory
     *                   default before any calibration is saved).
     * @pubstate writes defaultDabAntCap_. Does not itself re-tune.
     *
     * @author   Michele Bigi
     * @date     2026-08-20
     */
    void setDefaultDabAntCap(std::uint8_t antCap) noexcept;

    /**
     * @brief    tuneFm — tune to an FM centre frequency.
     *
     * @dname    tuneFm
     * @param    frequency  Validated FM centre frequency.
     * @param    antCap     Front-end antenna varactor override for this one
     *                      tune (e.g. for a calibration sweep). Omit to use
     *                      defaultFmAntCap_ (the board's saved calibration,
     *                      or hardware auto-tune if never calibrated).
     * @return   Ok on success, or a TunerError from ITuner.
     * @pubstate writes lastFmFrequency_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneFm(
        core::FrequencyKHz frequency,
        std::optional<std::uint8_t> antCap = std::nullopt);

    /**
     * @brief    setDefaultFmAntCap — set the board's calibrated ANTCAP.
     *
     * @dname    setDefaultFmAntCap
     * @param    antCap  Value applied to every FM tune that doesn't pass an
     *                   explicit override (0 = chip auto-tune, the factory
     *                   default before any calibration is saved).
     * @pubstate writes defaultFmAntCap_. Does not itself re-tune.
     *
     * @author   Michele Bigi
     * @date     2026-08-19
     */
    void setDefaultFmAntCap(std::uint8_t antCap) noexcept;

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
     * @brief    scanForStation — FM seek or DAB ensemble search with optional name.
     *
     * @dname    scanForStation
     * @param    request  Band, step limit, and optional label substring.
     * @return   Scan outcome; TunerError only on driver failure.
     * @pubstate tunes/plays first matching station; updates last tune caches.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<core::TunerScanResult, core::TunerError>
    scanForStation(const core::TunerScanRequest& request);

    /**
     * @brief    scanFullFmBand — sweep the whole FM band via hardware seek.
     *
     * @dname    scanFullFmBand
     * @return   Every station found, ascending frequency, or a TunerError.
     * @pubstate tunes to the band bottom then seeks up repeatedly until the
     *           sweep wraps back around; leaves the tuner parked wherever
     *           the last seek landed. Does not modify saved presets.
     *
     * @author   Michele Bigi
     * @date     2026-08-18
     */
    [[nodiscard]] std::expected<std::vector<core::TunerFmScannedStation>,
                                core::TunerError>
    scanFullFmBand();

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
     * @pubstate delegates to tuner_; caches service/component for status JSON.
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
     * @brief    refreshSignal — lock and signal metrics only.
     *
     * @dname    refreshSignal
     * @return   TunerStatus without RDS / DLS fields, or a TunerError.
     * @pubstate reads tuner_ under the tuner lock.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::expected<core::TunerStatus, core::TunerError>
    refreshSignal();

    /**
     * @brief    readDabLinkage — service-following data for a DAB service.
     *
     * @dname    readDabLinkage
     * @param    serviceId  Service playing in the current ensemble.
     * @return   core::DabLinkage, or a TunerError from ITuner.
     * @pubstate reads tuner_ under the tuner lock.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::expected<core::DabLinkage, core::TunerError>
    readDabLinkage(std::uint32_t serviceId);

    /**
     * @brief    recordDabFollow — note a DAB service-following attempt.
     *
     * @dname    recordDabFollow
     * @param    switched  true when the service was found elsewhere.
     * @pubstate increments afStats_.dabHunts (and dabSwitches).
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void recordDabFollow(bool switched);

    /**
     * @brief    setDabReturnMinutes — interval of FM→DAB return attempts.
     *
     * @dname    setDabReturnMinutes
     * @param    minutes  0 (never) to core::kMaxDabReturnMinutes.
     * @pubstate writes afStats_.dabReturnMinutes (not persisted).
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void setDabReturnMinutes(std::uint16_t minutes);

    /**
     * @brief    setOnFmFallback — note whether linked FM replaces DAB now.
     *
     * @dname    setOnFmFallback
     * @param    active  true after DAB following moved to FM.
     * @pubstate writes afStats_.onFmFallback.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void setOnFmFallback(bool active);

    /**
     * @brief    recordDabReturn — note a successful return from FM to DAB.
     *
     * @dname    recordDabReturn
     * @pubstate increments afStats_.dabReturns.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void recordDabReturn();

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

    /**
     * @brief    mutex — lock guarding every tuner operation.
     *
     * @dname    mutex
     * @return   Recursive mutex; hold it to run several operations (an AF
     *           hunt) without the HTTP task retuning in between.
     * @pubstate reads mutex_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::recursive_mutex& mutex() noexcept;

    /**
     * @brief    setAfFollowEnabled — turn RDS AF following on or off.
     *
     * @dname    setAfFollowEnabled
     * @param    enabled  true lets the AF-follow task retune on weak signal.
     * @pubstate writes afStats_.enabled (not persisted; default on).
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void setAfFollowEnabled(bool enabled);

    /**
     * @brief    setSweepActive — mark a long tuner sweep (spectrum) running.
     *
     * @dname    setSweepActive
     * @param    active  true while a sweep owns the tuner.
     * @pubstate writes sweepActive_ (atomic, no lock).
     *
     * Background followers check sweepActive() and stand aside, so they
     * neither retune mid-sweep nor read the swept noise as a fading
     * station.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void setSweepActive(bool active) noexcept;

    /**
     * @brief    sweepActive — whether a spectrum sweep owns the tuner.
     *
     * @dname    sweepActive
     * @return   true between setSweepActive(true) and (false).
     * @pubstate reads sweepActive_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool sweepActive() const noexcept;

    /**
     * @brief    recordAfHunt — note that the AF task probed alternates.
     *
     * @dname    recordAfHunt
     * @pubstate increments afStats_.hunts.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void recordAfHunt();

    /**
     * @brief    recordAfSwitch — note an automatic AF retune.
     *
     * @dname    recordAfSwitch
     * @param    from  Frequency left because of a weak signal.
     * @param    to    Alternate frequency now playing.
     * @pubstate increments afStats_.switches; updates last tune target.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void recordAfSwitch(core::FrequencyKHz from, core::FrequencyKHz to);

    /**
     * @brief    afFollowStats — AF-follow state for GET /api/tuner/af.
     *
     * @dname    afFollowStats
     * @return   Enabled flag, switch count and last switch.
     * @pubstate reads afStats_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] core::AfFollowStats afFollowStats();

private:
    core::ITuner& tuner_;
    std::uint8_t lastDabIndex_;
    core::FrequencyKHz lastFmFrequency_;
    std::uint8_t volume_;
    std::uint8_t defaultFmAntCap_;
    std::uint8_t defaultDabAntCap_;
    std::optional<std::uint32_t> lastPlayedServiceId_;
    std::optional<std::uint32_t> lastPlayedComponentId_;
    core::AfFollowStats afStats_{};
    std::recursive_mutex mutex_;
    std::atomic<bool> sweepActive_{false};
};

} // namespace tuner
