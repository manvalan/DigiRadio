/**
 * @file    AlarmJson.hpp
 * @brief   JSON for GET/PUT /api/alarms and /api/system/sleep.
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

#include "core/AlarmSchedule.hpp"
#include "core/ParseError.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace core {

/** Clock and schedule figures that GET /api/alarms reports with the table. */
struct AlarmClockInfo {
    bool timeValid{false};          ///< SNTP has set the clock.
    std::string localTime;          ///< "YYYY-MM-DDTHH:MM", empty if invalid.
    std::optional<NextAlarm> next;  ///< Soonest armed row.
};

/**
 * @brief    serializeAlarmTableJson — the table only, as stored in NVS.
 *
 * @dname    serializeAlarmTableJson
 * @param    table  Alarm rows.
 * @return   `{"alarms":[…]}`.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string serializeAlarmTableJson(
    std::span<const AlarmEntry> table);

/**
 * @brief    serializeAlarmStatusJson — GET /api/alarms response.
 *
 * @dname    serializeAlarmStatusJson
 * @param    table  Alarm rows.
 * @param    clock  Clock validity, local time and next row.
 * @return   `{"time_valid":…,"local_time":…,"next":…,"alarms":[…]}`.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string serializeAlarmStatusJson(
    std::span<const AlarmEntry> table, const AlarmClockInfo& clock);

/**
 * @brief    parseAlarmTableJson — PUT /api/alarms body (or the NVS copy).
 *
 * Rows are validated: hour < 24, minute < 60, days ≤ 0x7F, volume
 * −60…+12 dB, duration ≤ 720 min, web radio URL http(s) and ≤ 200 bytes,
 * at most kMaxAlarms rows.
 *
 * @dname    parseAlarmTableJson
 * @param    json  `{"alarms":[…]}`.
 * @return   Rows, or ParseError (InvalidJson for out-of-range values).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::expected<std::vector<AlarmEntry>, ParseError>
parseAlarmTableJson(std::string_view json);

/**
 * @brief    parseSleepMinutesJson — POST /api/system/sleep body.
 *
 * @dname    parseSleepMinutesJson
 * @param    json  `{"minutes":N}`, N 0–720 (0 cancels).
 * @return   Minutes, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::expected<std::uint16_t, ParseError> parseSleepMinutesJson(
    std::string_view json);

/**
 * @brief    serializeSleepJson — GET/POST /api/system/sleep response.
 *
 * @dname    serializeSleepJson
 * @param    secondsLeft  Seconds until standby; 0 when no timer runs.
 * @return   `{"active":…,"seconds_left":N}`.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string serializeSleepJson(std::uint32_t secondsLeft);

} // namespace core
