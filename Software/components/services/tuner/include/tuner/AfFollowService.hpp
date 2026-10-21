/**
 * @file    AfFollowService.hpp
 * @brief   Follow the current FM station to a stronger RDS alternate frequency.
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

#include "core/AlternateFrequencyFollower.hpp"
#include "tuner/TunerService.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace tuner {

/**
 * @brief    AfFollowService — periodic RDS AF follow on the single Si4684.
 *
 * @dname    AfFollowService
 * @return   n/a (type)
 * @pubstate Borrows TunerService for the process lifetime and owns a
 *           core::AlternateFrequencyFollower. tick() is meant to run every
 *           couple of seconds from a background task, so following works
 *           with no app connected.
 *
 * DigiRadio has one tuner, so an AF can only be measured by briefly
 * retuning to it: when the signal is weak, tick() holds the tuner lock,
 * probes each AF for a short settle time, then stays on the best one
 * with the same PI (or goes back).
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
class AfFollowService {
public:
    /** Blocking delay used between retune and measurement. */
    using SleepFn = void (*)(std::uint32_t ms);

    /**
     * @brief    AfFollowService — bind to the tuner service.
     *
     * @dname    AfFollowService
     * @param    tuner  Tuner service (must outlive this object).
     * @param    sleep  Blocking millisecond delay (vTaskDelay on device).
     * @pubstate stores references; follower starts with no station.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    AfFollowService(TunerService& tuner, SleepFn sleep);

    /**
     * @brief    tick — refresh status and follow an AF if needed.
     *
     * @dname    tick
     * @param    nowMs  Monotonic time in milliseconds.
     * @pubstate reads the tuner (drains RDS); on a weak FM signal may
     *           retune to probe AFs and record a switch in TunerService.
     *
     * @author   Michele Bigi
     * @date     2026-10-21
     */
    void tick(std::uint32_t nowMs);

private:
    void hunt(std::uint32_t nowMs);
    [[nodiscard]] std::vector<core::AfSample> probe(
        const std::vector<core::FrequencyKHz>& candidates);
    [[nodiscard]] std::optional<core::AfSample> measure(
        core::FrequencyKHz frequency);

    TunerService& tuner_;
    SleepFn sleep_;
    core::AlternateFrequencyFollower follower_;
};

} // namespace tuner
