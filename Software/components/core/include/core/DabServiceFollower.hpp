/**
 * @file    DabServiceFollower.hpp
 * @brief   Decide when and where to follow a DAB service (DAB or FM).
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

#include "core/DabLinkage.hpp"
#include "core/FrequencyKHz.hpp"
#include "core/TunerStatus.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace core {

/**
 * @brief    DabFollowCandidate — where the same programme may be found.
 *
 * @dname    DabFollowCandidate
 * @return   n/a (type)
 * @pubstate Plain DTO: a DAB ensemble index carrying the same SId, or an
 *           FM frequency carrying a hard-linked RDS PI.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct DabFollowCandidate {
    bool fm;                        ///< false: DAB ensemble, true: FM.
    std::uint8_t dabFreqIndex;      ///< Ensemble index (DAB only).
    std::optional<FrequencyKHz> fmFrequency; ///< FM frequency (FM only).
    std::uint16_t pi;               ///< Linked RDS PI (FM only).
};

/**
 * @brief    DabFollowConfig — thresholds for DAB service following.
 *
 * @dname    DabFollowConfig
 * @return   n/a (type)
 * @pubstate Plain DTO.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct DabFollowConfig {
    std::uint8_t weakFicQuality = 25U;  ///< FIC quality below this is weak.
    std::uint8_t weakObservations = 3U; ///< Consecutive weak snapshots.
    std::uint32_t settleMs = 8000U;     ///< Listen time after a new service.
    std::uint32_t cooldownMs = 30000U;  ///< Minimum gap between hunts.
    std::uint32_t linkageMaxAgeMs = 60000U; ///< Refresh linkage this often.
    std::size_t maxCandidates = 6U;     ///< Candidates tried per hunt.
};

/**
 * @brief    DabServiceFollower — DAB service-following policy (pure).
 *
 * @dname    DabServiceFollower
 * @return   n/a (type)
 * @pubstate Tracks the playing service from periodic DAB TunerStatus
 *           snapshots plus the FIG 0/6, 0/24 and 0/21 linkage read while
 *           the signal is good; on a sustained weak signal lists the other
 *           ensembles carrying the same SId, then FM frequencies of
 *           hard-linked RDS PI codes. The caller tunes and verifies.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
class DabServiceFollower {
public:
    /**
     * @brief    DabServiceFollower — construct with thresholds.
     *
     * @dname    DabServiceFollower
     * @param    config  Weak threshold, settle, cooldown and limits.
     * @pubstate starts with no service tracked.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    explicit DabServiceFollower(DabFollowConfig config = {});

    /**
     * @brief    observe — ingest a periodic DAB status snapshot.
     *
     * @dname    observe
     * @param    status  Latest tuner status (ignored unless DAB with a
     *                   playing service).
     * @param    nowMs   Monotonic time in milliseconds.
     * @pubstate a different ensemble or service starts a new service
     *           (linkage cleared); updates the weak-signal streak.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void observe(const TunerStatus& status, std::uint32_t nowMs);

    /**
     * @brief    needsLinkage — whether to (re)read the linkage now.
     *
     * @dname    needsLinkage
     * @param    nowMs  Monotonic time in milliseconds.
     * @return   true while a service plays with a good signal and the
     *           linkage is missing or older than linkageMaxAgeMs.
     * @pubstate reads tracked state.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] bool needsLinkage(std::uint32_t nowMs) const;

    /**
     * @brief    setLinkage — store linkage read for the playing service.
     *
     * @dname    setLinkage
     * @param    linkage  FIG 0/6, 0/24 and 0/21 data from the tuner.
     * @param    nowMs    Monotonic time in milliseconds.
     * @pubstate replaces linkage_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void setLinkage(DabLinkage linkage, std::uint32_t nowMs);

    /**
     * @brief    shouldHunt — whether to look elsewhere now.
     *
     * @dname    shouldHunt
     * @param    nowMs  Monotonic time in milliseconds.
     * @return   true after weakObservations weak snapshots in a row, past
     *           the settle and cooldown windows, with at least one
     *           candidate known.
     * @pubstate reads tracked state.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] bool shouldHunt(std::uint32_t nowMs) const;

    /**
     * @brief    candidates — where to look, DAB ensembles first, then FM.
     *
     * @dname    candidates
     * @return   Up to maxCandidates entries, duplicates and the current
     *           ensemble removed.
     * @pubstate reads linkage_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::vector<DabFollowCandidate> candidates() const;

    /**
     * @brief    serviceId — SId of the service being followed.
     *
     * @dname    serviceId
     * @return   Playing service id, or nullopt.
     * @pubstate reads serviceId_.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    [[nodiscard]] std::optional<std::uint32_t> serviceId() const;

    /**
     * @brief    finishHunt — start the cooldown after a hunt.
     *
     * @dname    finishHunt
     * @param    nowMs  Monotonic time in milliseconds.
     * @pubstate sets lastHuntMs_; clears the weak streak.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void finishHunt(std::uint32_t nowMs);

private:
    [[nodiscard]] bool weak(const TunerStatus& status) const;
    [[nodiscard]] std::vector<std::uint16_t> hardLinkedPis() const;
    void addDabCandidates(std::vector<DabFollowCandidate>& out) const;
    void addFmCandidates(std::vector<DabFollowCandidate>& out) const;

    DabFollowConfig config_;
    std::optional<std::uint32_t> serviceId_;
    std::optional<std::uint8_t> freqIndex_;
    std::optional<DabLinkage> linkage_;
    std::uint32_t linkageAtMs_{0U};
    std::uint32_t serviceSinceMs_{0U};
    std::optional<std::uint32_t> lastHuntMs_;
    std::uint8_t weakStreak_{0U};
};

} // namespace core
