/**
 * @file    alarm_schedule_test.cpp
 * @brief   Host tests for the alarm table: scheduling, ramp and JSON.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/AlarmJson.hpp"
#include "core/AlarmSchedule.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] bool expect(bool ok, const char* what)
{
    if (!ok) {
        std::cerr << "FAILED: " << what << '\n';
    }
    return ok;
}

[[nodiscard]] core::AlarmEntry weekdayAlarm()
{
    core::AlarmEntry entry;
    entry.hour = 7U;
    entry.minute = 30U;
    entry.days = 0x1FU; // Monday–Friday
    return entry;
}

[[nodiscard]] bool scheduleTest()
{
    const std::vector<core::AlarmEntry> table{weekdayAlarm()};
    const core::LocalTime friday730{.weekday = 4U, .hour = 7U, .minute = 30U};
    const core::LocalTime saturday730{.weekday = 5U, .hour = 7U, .minute = 30U};
    const core::LocalTime friday800{.weekday = 4U, .hour = 8U, .minute = 0U};
    const auto next = core::nextAlarm(table, friday800);
    return expect(core::dueAlarm(table, friday730) == 0U, "due on Friday")
           && expect(!core::dueAlarm(table, saturday730), "not on Saturday")
           // Friday 08:00 -> Monday 07:30 = 2 days + 23.5 h.
           && expect(next && next->minutes == 2U * 1440U + 1410U,
                     "next is Monday 07:30")
           && expect(core::weekdayFromTm(0) == 6U, "tm Sunday is 6")
           && expect(core::weekdayFromTm(1) == 0U, "tm Monday is 0");
}

[[nodiscard]] bool disabledTest()
{
    auto entry = weekdayAlarm();
    entry.enabled = false;
    const std::vector<core::AlarmEntry> table{entry};
    const core::LocalTime monday{.weekday = 0U, .hour = 7U, .minute = 30U};
    return expect(!core::dueAlarm(table, monday), "disabled never fires")
           && expect(!core::nextAlarm(table, monday), "no next when disabled");
}

[[nodiscard]] bool rampTest()
{
    const float start = core::alarmRampDb(-10.0F, 0U, 30U);
    const float half = core::alarmRampDb(-10.0F, 15U, 30U);
    const float end = core::alarmRampDb(-10.0F, 40U, 30U);
    return expect(std::fabs(start - core::kAlarmRampStartDb) < 0.01F, "start")
           && expect(std::fabs(half + 27.5F) < 0.01F, "half way")
           && expect(std::fabs(end + 10.0F) < 0.01F, "target")
           && expect(core::alarmRampDb(-50.0F, 0U, 30U) == -50.0F,
                     "quiet target at once");
}

[[nodiscard]] bool jsonRoundTripTest()
{
    auto on = weekdayAlarm();
    on.source = core::AlarmSource::WebRadio;
    on.url = "http://icestreaming.rai.it/1.mp3";
    on.volumeDb = -12.5F;
    on.durationMinutes = 60U;
    core::AlarmEntry off;
    off.action = core::AlarmAction::TurnOff;
    off.hour = 23U;
    const std::vector<core::AlarmEntry> table{on, off};
    const auto parsed =
        core::parseAlarmTableJson(core::serializeAlarmTableJson(table));
    return expect(parsed.has_value() && parsed->size() == 2U, "parses back")
           && expect((*parsed)[0].url == on.url, "url")
           && expect((*parsed)[0].durationMinutes == 60U, "duration")
           && expect(std::fabs((*parsed)[0].volumeDb + 12.5F) < 0.01F, "volume")
           && expect((*parsed)[1].action == core::AlarmAction::TurnOff, "off")
           && expect((*parsed)[1].hour == 23U, "off hour");
}

[[nodiscard]] bool jsonRejectsTest()
{
    const auto badHour = core::parseAlarmTableJson(
        R"({"alarms":[{"action":"on","hour":24,"minute":0,"days":127}]})");
    const auto badUrl = core::parseAlarmTableJson(
        R"({"alarms":[{"hour":7,"minute":0,"days":1,"source":"web","url":"ftp://x"}]})");
    const auto empty = core::parseAlarmTableJson(R"({"alarms":[]})");
    const auto missing = core::parseAlarmTableJson(R"({"x":1})");
    return expect(!badHour, "hour 24 rejected")
           && expect(!badUrl, "ftp url rejected")
           && expect(empty && empty->empty(), "empty table ok")
           && expect(!missing, "missing alarms rejected");
}

[[nodiscard]] bool statusAndSleepTest()
{
    const std::vector<core::AlarmEntry> table{weekdayAlarm()};
    core::AlarmClockInfo clock;
    clock.timeValid = true;
    clock.localTime = "2026-10-26T17:00";
    clock.next = core::NextAlarm{.index = 0U, .minutes = 870U};
    const std::string json = core::serializeAlarmStatusJson(table, clock);
    const auto minutes = core::parseSleepMinutesJson(R"({"minutes":30})");
    return expect(json.find(R"("next":{"index":0,"in_min":870})")
                      != std::string::npos,
                  "status has next")
           && expect(minutes && *minutes == 30U, "sleep minutes")
           && expect(!core::parseSleepMinutesJson(R"({"minutes":9999})"),
                     "sleep too long")
           && expect(core::serializeSleepJson(0U)
                         == R"({"active":false,"seconds_left":0})",
                     "sleep idle json");
}

} // namespace

int main()
{
    const bool ok = scheduleTest() && disabledTest() && rampTest()
                    && jsonRoundTripTest() && jsonRejectsTest()
                    && statusAndSleepTest();
    if (!ok) {
        return EXIT_FAILURE;
    }
    std::cout << "alarm_schedule_test OK\n";
    return EXIT_SUCCESS;
}
