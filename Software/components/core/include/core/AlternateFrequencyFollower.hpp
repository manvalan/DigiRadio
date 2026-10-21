/**
 * @file    AlternateFrequencyFollower.hpp
 * @brief   Decide when and where to follow an FM station via its RDS AF list.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */
#pragma once

#include "core/FrequencyKHz.hpp"
#include "core/TunerStatus.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace core {

/**
 * @brief    AfSample — signal quality measured on one FM frequency.
 *
 * @dname    AfSample
 * @return   n/a (type)
 * @pubstate Plain DTO filled from a TunerStatus taken on that frequency.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct AfSample {
    FrequencyKHz frequency;            ///< Measured FM frequency.
    std::int8_t rssiDbuV;              ///< RSSI in dBµV (-128 when unknown).
    bool locked;                       ///< FM_RSQ VALID flag.
    std::optional<std::uint16_t> pi;   ///< Confirmed RDS PI, if decoded.
};

/**
 * @brief    AfFollowConfig — thresholds for alternate-frequency following.
 *
 * @dname    AfFollowConfig
 * @return   n/a (type)
 * @pubstate Plain DTO; defaults match the former iOS app heuristics.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct AfFollowConfig {
    std::int8_t weakRssiDbuV = 12;      ///< Below this, look for an AF.
    std::int8_t minRssiDbuV = 5;        ///< An AF must reach at least this.
    std::int8_t improvementDb = 6;      ///< Required gain over current.
    std::uint32_t settleMs = 8000U;     ///< Listen time after a new station.
    std::uint32_t cooldownMs = 20000U;  ///< Minimum gap between hunts.
    std::size_t maxCandidates = 6U;     ///< AFs probed per hunt.
    std::uint8_t weakObservations = 3U; ///< Consecutive weak samples needed.
};

/**
 * @brief    AfFollowStats — AF-follow state reported by GET /api/tuner/af.
 *
 * @dname    AfFollowStats
 * @return   n/a (type)
 * @pubstate Plain DTO kept by tuner::TunerService.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct AfFollowStats {
    bool enabled = true;                  ///< Automatic AF following on.
    std::uint32_t hunts = 0U;             ///< AF probes since boot.
    std::uint32_t switches = 0U;          ///< Automatic retunes since boot.
    std::optional<FrequencyKHz> lastFrom; ///< Frequency left last time.
    std::optional<FrequencyKHz> lastTo;   ///< AF switched to last time.
    std::uint32_t dabHunts = 0U;          ///< DAB service-following tries.
    std::uint32_t dabSwitches = 0U;       ///< DAB services found elsewhere.
};

/**
 * @brief    AlternateFrequencyFollower — RDS AF switching policy (pure).
 *
 * @dname    AlternateFrequencyFollower
 * @return   n/a (type)
 * @pubstate Tracks the current station (PI + AF set, including the
 *           frequencies it already used) from periodic TunerStatus
 *           snapshots, decides when the signal is weak enough to hunt and
 *           picks the best probed AF. The caller does the tuning.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
class AlternateFrequencyFollower {
public:
    /**
     * @brief    AlternateFrequencyFollower — construct with thresholds.
     *
     * @dname    AlternateFrequencyFollower
     * @param    config  Weak/min RSSI, gain, settle and cooldown settings.
     * @pubstate starts with no station tracked.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    explicit AlternateFrequencyFollower(AfFollowConfig config = {});

    /**
     * @brief    observe — ingest a periodic FM status snapshot.
     *
     * @dname    observe
     * @param    status  Latest tuner status (ignored unless FM).
     * @param    nowMs   Monotonic time in milliseconds.
     * @pubstate a frequency outside the tracked AF set starts a new
     *           station (AF set and PI cleared, settle timer restarted);
     *           otherwise merges the RDS AF list and PI into the station.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void observe(const TunerStatus& status, std::uint32_t nowMs);

    /**
     * @brief    shouldHunt — whether to probe alternate frequencies now.
     *
     * @dname    shouldHunt
     * @param    nowMs  Monotonic time in milliseconds.
     * @return   true when the signal was weak for weakObservations
     *           consecutive snapshots (a lone VALID/RSSI dip never hunts),
     *           AFs are known and the settle and cooldown windows elapsed.
     * @pubstate reads tracked station state.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] bool shouldHunt(std::uint32_t nowMs) const;

    /**
     * @brief    candidates — AFs to probe, current frequency excluded.
     *
     * @dname    candidates
     * @return   Up to maxCandidates frequencies, in RDS order.
     * @pubstate reads afSet_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::vector<FrequencyKHz> candidates() const;

    /**
     * @brief    currentSample — quality of the frequency now playing.
     *
     * @dname    currentSample
     * @return   Last observed sample, or nullopt before any FM status.
     * @pubstate reads current_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::optional<AfSample> currentSample() const;

    /**
     * @brief    choose — pick where to stay after probing.
     *
     * @dname    choose
     * @param    current  Sample of the frequency the hunt started from.
     * @param    probed   Samples measured on the candidate AFs.
     * @return   The best usable same-PI AF when it beats current by the
     *           required gain (or current is unusable), else current.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] FrequencyKHz choose(const AfSample& current,
                                      std::span<const AfSample> probed) const;

    /**
     * @brief    finishHunt — record the outcome of a hunt.
     *
     * @dname    finishHunt
     * @param    chosen  Frequency the tuner was left on.
     * @param    nowMs   Monotonic time in milliseconds.
     * @pubstate starts the cooldown; keeps the station when chosen is one
     *           of its frequencies.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void finishHunt(FrequencyKHz chosen, std::uint32_t nowMs);

private:
    [[nodiscard]] bool inStation(std::uint32_t khz) const;
    [[nodiscard]] bool usable(const AfSample& sample) const;
    [[nodiscard]] bool samePi(const AfSample& sample) const;
    [[nodiscard]] bool better(const AfSample& lhs, const AfSample& rhs) const;
    void startStation(const AfSample& sample, std::uint32_t nowMs);
    void addFrequency(std::uint32_t khz);
    void mergeStatus(const TunerStatus& status);

    AfFollowConfig config_;
    std::optional<AfSample> current_;
    std::optional<std::uint16_t> stationPi_;
    std::vector<std::uint32_t> afSet_;
    std::uint32_t stationSinceMs_{0U};
    std::optional<std::uint32_t> lastHuntMs_;
    std::uint8_t weakStreak_{0U};
};

} // namespace core
