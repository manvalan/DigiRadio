/**
 * @file    AlarmSchedule.cpp
 * @brief   Alarm table scheduling: due row, next row, wake-up ramp.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/AlarmSchedule.hpp"

namespace core {

namespace {

constexpr std::uint32_t kMinutesPerDay = 24U * 60U;
constexpr std::uint8_t kDaysPerWeek = 7U;

[[nodiscard]] bool firesOnDay(const AlarmEntry& entry,
                              std::uint8_t weekday) noexcept
{
    return (entry.days & (1U << weekday)) != 0U;
}

/** Minutes from `now` to the row's next firing, nullopt if never. */
[[nodiscard]] std::optional<std::uint32_t> minutesUntil(
    const AlarmEntry& entry, const LocalTime& now) noexcept
{
    if (!entry.enabled || (entry.days & kAllWeekdays) == 0U) {
        return std::nullopt;
    }
    const std::uint32_t nowMin = now.hour * 60U + now.minute;
    const std::uint32_t atMin = entry.hour * 60U + entry.minute;
    for (std::uint8_t ahead = 0U; ahead <= kDaysPerWeek; ++ahead) {
        const auto day = static_cast<std::uint8_t>((now.weekday + ahead) % 7U);
        const std::uint32_t when = ahead * kMinutesPerDay + atMin;
        if (when >= nowMin && firesOnDay(entry, day)) {
            return when - nowMin;
        }
    }
    return std::nullopt;
}

} // namespace

std::uint8_t weekdayFromTm(int tmWday) noexcept
{
    return static_cast<std::uint8_t>((tmWday + 6) % 7);
}

bool alarmMatches(const AlarmEntry& entry, const LocalTime& now) noexcept
{
    return entry.enabled && entry.hour == now.hour
           && entry.minute == now.minute && firesOnDay(entry, now.weekday);
}

std::optional<std::size_t> dueAlarm(std::span<const AlarmEntry> table,
                                    const LocalTime& now) noexcept
{
    for (std::size_t i = 0U; i < table.size(); ++i) {
        if (alarmMatches(table[i], now)) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<NextAlarm> nextAlarm(std::span<const AlarmEntry> table,
                                   const LocalTime& now) noexcept
{
    std::optional<NextAlarm> best;
    for (std::size_t i = 0U; i < table.size(); ++i) {
        const auto minutes = minutesUntil(table[i], now);
        if (minutes && (!best || *minutes < best->minutes)) {
            best = NextAlarm{.index = i, .minutes = *minutes};
        }
    }
    return best;
}

float alarmRampDb(float targetDb, std::uint32_t elapsedSec,
                  std::uint32_t rampSeconds) noexcept
{
    if (targetDb <= kAlarmRampStartDb || rampSeconds == 0U
        || elapsedSec >= rampSeconds) {
        return targetDb;
    }
    const float fraction =
        static_cast<float>(elapsedSec) / static_cast<float>(rampSeconds);
    return kAlarmRampStartDb + (targetDb - kAlarmRampStartDb) * fraction;
}

} // namespace core
