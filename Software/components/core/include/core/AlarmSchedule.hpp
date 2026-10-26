/**
 * @file    AlarmSchedule.hpp
 * @brief   Alarm table (turn on / turn off at a time) and its scheduling.
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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace core {

/** What an alarm row does when it fires. */
enum class AlarmAction {
    TurnOn,  ///< Leave standby, tune the source, ramp the volume up.
    TurnOff, ///< Go to standby and stop the web radio.
};

/** What a TurnOn row plays. */
enum class AlarmSource {
    Preset,   ///< A saved station, by its position in the preset list.
    WebRadio, ///< An http(s) MP3 stream URL.
};

/** Upper bound on rows, so the table fits one small NVS blob. */
inline constexpr std::size_t kMaxAlarms = 8U;

/** Weekday bits of AlarmEntry::days: bit 0 Monday … bit 6 Sunday. */
inline constexpr std::uint8_t kAllWeekdays = 0x7FU;

/** One row of the alarm table. */
struct AlarmEntry {
    bool enabled{true};                   ///< Off rows never fire.
    AlarmAction action{AlarmAction::TurnOn}; ///< On or off.
    std::uint8_t hour{7U};                ///< Local time, 0–23.
    std::uint8_t minute{0U};              ///< 0–59.
    std::uint8_t days{kAllWeekdays};      ///< Weekday mask, bit 0 = Monday.
    AlarmSource source{AlarmSource::Preset}; ///< TurnOn only.
    std::uint8_t preset{0U};              ///< Preset index (Preset source).
    std::string url;                      ///< Stream URL (WebRadio source).
    float volumeDb{-20.0F};               ///< Master gain reached by the ramp.
    std::uint16_t durationMinutes{0U};    ///< TurnOn: standby after this; 0 = stay on.
};

/** Wall-clock fields the scheduler needs, already in local time. */
struct LocalTime {
    std::uint8_t weekday{0U}; ///< 0 = Monday … 6 = Sunday.
    std::uint8_t hour{0U};    ///< 0–23.
    std::uint8_t minute{0U};  ///< 0–59.
};

/** The next row to fire and how far away it is. */
struct NextAlarm {
    std::size_t index{0U};     ///< Position in the table.
    std::uint32_t minutes{0U}; ///< Minutes from now (0 = this minute).
};

/**
 * @brief    weekdayFromTm — convert struct tm's tm_wday (0 = Sunday) to
 *           LocalTime::weekday (0 = Monday).
 *
 * @dname    weekdayFromTm
 * @param    tmWday  0–6, Sunday first.
 * @return   0–6, Monday first.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::uint8_t weekdayFromTm(int tmWday) noexcept;

/**
 * @brief    alarmMatches — whether an enabled row fires at this minute.
 *
 * @dname    alarmMatches
 * @param    entry  Alarm row.
 * @param    now    Local time.
 * @return   true when enabled, on today's weekday and at hour:minute.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool alarmMatches(const AlarmEntry& entry,
                                const LocalTime& now) noexcept;

/**
 * @brief    dueAlarm — first row that fires at this minute.
 *
 * @dname    dueAlarm
 * @param    table  Alarm rows.
 * @param    now    Local time.
 * @return   Row index, or nullopt.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<std::size_t> dueAlarm(
    std::span<const AlarmEntry> table, const LocalTime& now) noexcept;

/**
 * @brief    nextAlarm — the enabled row that fires soonest, within a week.
 *
 * @dname    nextAlarm
 * @param    table  Alarm rows.
 * @param    now    Local time.
 * @return   Index and minutes from now, or nullopt when nothing is armed.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<NextAlarm> nextAlarm(
    std::span<const AlarmEntry> table, const LocalTime& now) noexcept;

/**
 * @brief    alarmRampDb — master gain during the wake-up fade-in.
 *
 * Linear in dB from kAlarmRampStartDb to `targetDb` over `rampSeconds`;
 * a target below the start is reached at once.
 *
 * @dname    alarmRampDb
 * @param    targetDb     Final gain.
 * @param    elapsedSec   Seconds since the alarm fired.
 * @param    rampSeconds  Ramp length (> 0).
 * @return   Gain to apply now, in dB.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] float alarmRampDb(float targetDb, std::uint32_t elapsedSec,
                                std::uint32_t rampSeconds) noexcept;

/** Gain the fade-in starts from (barely audible). */
inline constexpr float kAlarmRampStartDb = -45.0F;

} // namespace core
