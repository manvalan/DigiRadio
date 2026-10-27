/**
 * @file    SpectrumScanService.hpp
 * @brief   Background RF band sweep for GET/POST /api/tuner/spectrum.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include "core/FrequencyKHz.hpp"
#include "core/RadioSpectrum.hpp"
#include "tuner/TunerService.hpp"

#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>
#include <string>

namespace tuner {

/**
 * @brief    SpectrumScanService — sweeps FM or DAB Band III and keeps the
 *           measured levels.
 *
 * @dname    SpectrumScanService
 * @param    tuner  Tuner service, borrowed for the process lifetime.
 * @return   n/a (type)
 * @pubstate Public interface: start(band), json(). Owns the last
 *           SpectrumSnapshot behind its own mutex, so progress can be
 *           read while the sweep holds the tuner.
 *
 * The Si4684 has one front end, so a sweep retunes the station: start()
 * spawns a short-lived task that mutes the tuner, marks the sweep active
 * (AF/DAB following stands aside), measures each FM step (100 kHz,
 * RSSI + SNR) or each Band III channel (RSSI, plus CNR/FIC when an
 * ensemble locks), then retunes the previous station and volume. Only
 * one sweep runs at a time. Each step takes the tuner lock on its own,
 * so status requests keep being served during the ~30-60 s sweep.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class SpectrumScanService {
public:
    /**
     * @brief    SpectrumScanService — bind to the tuner service.
     *
     * @dname    SpectrumScanService
     * @param    tuner  Tuner service (must outlive this object).
     * @pubstate stores the reference; snapshot starts Idle.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    explicit SpectrumScanService(TunerService& tuner);

    /**
     * @brief    start — begin a sweep of band in the background.
     *
     * @dname    start
     * @param    band  Band to sweep.
     * @return   Ok when the sweep task started, or SpectrumStartError::Busy
     *           (one already running) / NoResource (task not created).
     * @pubstate resets snapshot_ to Running with no points; spawns the
     *           sweep task.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::expected<void, core::SpectrumStartError> start(
        core::TunerBand band);

    /**
     * @brief    json — current state, progress and points as JSON.
     *
     * @dname    json
     * @return   core::serializeSpectrumJson() of the snapshot.
     * @pubstate reads snapshot_ under mutex_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::string json();

private:
    struct Origin {
        core::TunerBand band;
        std::optional<core::FrequencyKHz> fmFrequency;
        std::uint8_t dabIndex;
        std::optional<std::uint32_t> serviceId;
        std::optional<std::uint32_t> componentId;
        std::uint8_t volume;
    };

    static void taskEntry(void* arg);
    void run();
    [[nodiscard]] bool sweepFm();
    [[nodiscard]] bool sweepDab();
    void record(const core::SpectrumPoint& point, std::size_t total);
    void finish(bool ok);
    [[nodiscard]] std::optional<Origin> captureOrigin();
    void restore(const Origin& origin);

    TunerService& tuner_;
    std::mutex mutex_;
    core::SpectrumSnapshot snapshot_;
};

} // namespace tuner
