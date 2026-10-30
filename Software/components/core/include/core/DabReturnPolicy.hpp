/**
 * @file    DabReturnPolicy.hpp
 * @brief   When to retry DAB after service following fell back to FM.
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

#include "core/TunerStatus.hpp"

#include <cstdint>
#include <optional>

namespace core {

/** Default minutes between DAB return attempts (0 disables them). */
inline constexpr std::uint16_t kDefaultDabReturnMinutes = 10U;

/** Longest accepted interval between DAB return attempts. */
inline constexpr std::uint16_t kMaxDabReturnMinutes = 120U;

/** FIC quality needed to go back to DAB: well above the 25 that counts
 *  as weak for DabServiceFollower, so the radio does not bounce. */
inline constexpr std::uint8_t kDabReturnMinFicQuality = 60U;

/**
 * @brief    DabReturnTarget — the DAB service left for FM.
 *
 * @dname    DabReturnTarget
 * @return   n/a (type)
 * @pubstate Plain DTO.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct DabReturnTarget {
    std::uint8_t freqIndex{0U};    ///< Ensemble the service was on.
    std::uint32_t serviceId{0U};   ///< DAB SId.
    std::uint32_t componentId{0U}; ///< Audio component.
    std::uint16_t pi{0U};          ///< Hard-linked RDS PI now playing.
};

/**
 * @brief    DabReturnPolicy — schedule for leaving the FM fallback.
 *
 * @dname    DabReturnPolicy
 * @param    none
 * @return   n/a (type)
 * @pubstate Public interface: setIntervalMinutes(), arm(), cancel(),
 *           target(), isStale(), shouldTry(), recordAttempt(),
 *           dabGoodEnough(). Holds the target and the last attempt time.
 *
 * One tuner means measuring DAB interrupts the FM audio (~1-2 s), so a
 * return is tried only every N minutes. The fallback ends when the
 * listener moves away (another band, or FM with a different PI); AF
 * switches to the same PI keep it.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class DabReturnPolicy {
public:
    /**
     * @brief    setIntervalMinutes — minutes between attempts (0 = off).
     *
     * @dname    setIntervalMinutes
     * @param    minutes  0–kMaxDabReturnMinutes (larger values clamp).
     * @pubstate writes minutes_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void setIntervalMinutes(std::uint16_t minutes) noexcept;

    /**
     * @brief    arm — remember the DAB service just left for FM.
     *
     * @dname    arm
     * @param    target  Service and the FM PI that replaced it.
     * @param    nowMs   Monotonic time; the first try is one interval later.
     * @pubstate writes target_ and lastTryMs_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void arm(const DabReturnTarget& target, std::uint32_t nowMs) noexcept;

    /**
     * @brief    cancel — end the fallback (returned, or listener moved).
     *
     * @dname    cancel
     * @pubstate clears target_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void cancel() noexcept;

    /**
     * @brief    target — the DAB service to return to, if any.
     *
     * @dname    target
     * @return   The armed target, or nullopt.
     * @pubstate reads target_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] const std::optional<DabReturnTarget>& target() const noexcept;

    /**
     * @brief    isStale — whether the listener left the FM fallback.
     *
     * @dname    isStale
     * @param    status  Latest tuner snapshot.
     * @return   true when armed and the band is not FM, or a confirmed PI
     *           differs from the target's.
     * @pubstate reads target_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool isStale(const TunerStatus& status) const noexcept;

    /**
     * @brief    shouldTry — whether to attempt the return now.
     *
     * @dname    shouldTry
     * @param    status  Latest tuner snapshot.
     * @param    nowMs   Monotonic time.
     * @return   true when armed, not stale, the interval is non-zero and
     *           has elapsed since arming or the last attempt.
     * @pubstate reads target_, minutes_ and lastTryMs_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] bool shouldTry(const TunerStatus& status,
                                 std::uint32_t nowMs) const noexcept;

    /**
     * @brief    recordAttempt — restart the interval after a try.
     *
     * @dname    recordAttempt
     * @param    nowMs  Monotonic time of the attempt.
     * @pubstate writes lastTryMs_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void recordAttempt(std::uint32_t nowMs) noexcept;

    /**
     * @brief    dabGoodEnough — whether a DAB snapshot justifies leaving FM.
     *
     * @dname    dabGoodEnough
     * @param    status  Snapshot taken on the target ensemble.
     * @return   true when DAB, locked and FIC ≥ kDabReturnMinFicQuality.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] static bool dabGoodEnough(const TunerStatus& status) noexcept;

private:
    std::optional<DabReturnTarget> target_;
    std::uint32_t lastTryMs_{0U};
    std::uint16_t minutes_{kDefaultDabReturnMinutes};
};

} // namespace core
