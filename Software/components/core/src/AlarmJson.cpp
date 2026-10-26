/**
 * @file    AlarmJson.cpp
 * @brief   JSON for GET/PUT /api/alarms and /api/system/sleep.
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

#include <charconv>
#include <cstdio>
#include <sstream>

namespace core {

namespace {

constexpr std::size_t kMaxUrlLength = 200U;
constexpr float kMinVolumeDb = -60.0F;
constexpr float kMaxVolumeDb = 12.0F;
constexpr std::uint16_t kMaxMinutes = 720U;

/** Text after `"key":` with leading blanks skipped, empty if absent. */
[[nodiscard]] std::string_view valueOf(std::string_view object,
                                       std::string_view key)
{
    const std::string needle = std::string("\"") + std::string(key) + "\":";
    const std::size_t at = object.find(needle);
    if (at == std::string_view::npos) {
        return {};
    }
    std::string_view rest = object.substr(at + needle.size());
    while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\n')) {
        rest.remove_prefix(1U);
    }
    return rest;
}

[[nodiscard]] std::optional<long> numberOf(std::string_view object,
                                           std::string_view key)
{
    const std::string_view rest = valueOf(object, key);
    long value = 0;
    const auto [end, ec] =
        std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (ec != std::errc{} || end == rest.data()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<float> floatOf(std::string_view object,
                                           std::string_view key)
{
    const std::string_view rest = valueOf(object, key);
    float value = 0.0F;
    const auto [end, ec] =
        std::from_chars(rest.data(), rest.data() + rest.size(), value);
    if (ec != std::errc{} || end == rest.data()) {
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<bool> boolOf(std::string_view object,
                                         std::string_view key)
{
    const std::string_view rest = valueOf(object, key);
    if (rest.starts_with("true")) {
        return true;
    }
    if (rest.starts_with("false")) {
        return false;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string_view> stringOf(
    std::string_view object, std::string_view key)
{
    const std::string_view rest = valueOf(object, key);
    if (rest.empty() || rest.front() != '"') {
        return std::nullopt;
    }
    const std::size_t end = rest.find('"', 1U);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return rest.substr(1U, end - 1U);
}

[[nodiscard]] bool plainUrl(std::string_view url)
{
    if (url.size() > kMaxUrlLength
        || !(url.starts_with("http://") || url.starts_with("https://"))) {
        return false;
    }
    for (const char c : url) {
        if (static_cast<unsigned char>(c) < 0x20U || c == '\\') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool inRange(std::optional<long> value, long max)
{
    return value && *value >= 0 && *value <= max;
}

/** Fields shared by both actions: when the row fires. */
[[nodiscard]] bool parseWhen(std::string_view object, AlarmEntry& entry)
{
    const auto hour = numberOf(object, "hour");
    const auto minute = numberOf(object, "minute");
    const auto days = numberOf(object, "days");
    if (!inRange(hour, 23) || !inRange(minute, 59)
        || !inRange(days, kAllWeekdays)) {
        return false;
    }
    entry.hour = static_cast<std::uint8_t>(*hour);
    entry.minute = static_cast<std::uint8_t>(*minute);
    entry.days = static_cast<std::uint8_t>(*days);
    entry.enabled = boolOf(object, "enabled").value_or(true);
    return true;
}

[[nodiscard]] bool parseSource(std::string_view object, AlarmEntry& entry)
{
    const auto source = stringOf(object, "source").value_or("preset");
    if (source == "web") {
        const auto url = stringOf(object, "url");
        entry.source = AlarmSource::WebRadio;
        entry.url = url ? std::string(*url) : std::string();
        return plainUrl(entry.url);
    }
    const auto preset = numberOf(object, "preset");
    entry.source = AlarmSource::Preset;
    entry.preset = static_cast<std::uint8_t>(preset.value_or(0));
    return source == "preset" && inRange(preset.value_or(0), 63);
}

/** TurnOn extras: what to play, how loud, for how long. */
[[nodiscard]] bool parseTurnOn(std::string_view object, AlarmEntry& entry)
{
    const float volume = floatOf(object, "volume_db").value_or(-20.0F);
    const auto duration = numberOf(object, "duration_min").value_or(0);
    if (volume < kMinVolumeDb || volume > kMaxVolumeDb
        || !inRange(duration, kMaxMinutes)) {
        return false;
    }
    entry.volumeDb = volume;
    entry.durationMinutes = static_cast<std::uint16_t>(duration);
    return parseSource(object, entry);
}

[[nodiscard]] std::optional<AlarmEntry> parseEntry(std::string_view object)
{
    AlarmEntry entry;
    const auto action = stringOf(object, "action").value_or("on");
    if (action != "on" && action != "off") {
        return std::nullopt;
    }
    entry.action = action == "on" ? AlarmAction::TurnOn : AlarmAction::TurnOff;
    if (!parseWhen(object, entry)) {
        return std::nullopt;
    }
    if (entry.action == AlarmAction::TurnOn && !parseTurnOn(object, entry)) {
        return std::nullopt;
    }
    return entry;
}

/** Splits the array body into `{…}` objects (rows hold no nested objects). */
[[nodiscard]] std::optional<std::vector<std::string_view>> splitObjects(
    std::string_view array)
{
    std::vector<std::string_view> objects;
    std::size_t pos = 0U;
    while ((pos = array.find('{', pos)) != std::string_view::npos) {
        const std::size_t end = array.find('}', pos);
        if (end == std::string_view::npos || objects.size() == kMaxAlarms) {
            return std::nullopt;
        }
        objects.push_back(array.substr(pos, end - pos + 1U));
        pos = end + 1U;
    }
    return objects;
}

[[nodiscard]] std::string_view alarmsArray(std::string_view json)
{
    const std::string_view rest = valueOf(json, "alarms");
    if (rest.empty() || rest.front() != '[') {
        return {};
    }
    const std::size_t end = rest.find(']');
    return end == std::string_view::npos ? std::string_view{}
                                         : rest.substr(0U, end + 1U);
}

void writeEntry(std::ostringstream& out, const AlarmEntry& e)
{
    const bool on = e.action == AlarmAction::TurnOn;
    out << R"({"enabled":)" << (e.enabled ? "true" : "false")
        << R"(,"action":")" << (on ? "on" : "off") << '"'
        << R"(,"hour":)" << static_cast<int>(e.hour)
        << R"(,"minute":)" << static_cast<int>(e.minute)
        << R"(,"days":)" << static_cast<int>(e.days);
    if (on) {
        const bool web = e.source == AlarmSource::WebRadio;
        out << R"(,"source":")" << (web ? "web" : "preset") << '"'
            << R"(,"preset":)" << static_cast<int>(e.preset)
            << R"(,"url":")" << (web ? e.url : std::string()) << '"'
            << R"(,"volume_db":)" << e.volumeDb
            << R"(,"duration_min":)" << e.durationMinutes;
    }
    out << '}';
}

void writeTable(std::ostringstream& out, std::span<const AlarmEntry> table)
{
    out << R"("alarms":[)";
    for (std::size_t i = 0U; i < table.size(); ++i) {
        if (i != 0U) {
            out << ',';
        }
        writeEntry(out, table[i]);
    }
    out << ']';
}

} // namespace

std::string serializeAlarmTableJson(std::span<const AlarmEntry> table)
{
    std::ostringstream out;
    out << '{';
    writeTable(out, table);
    out << '}';
    return out.str();
}

std::string serializeAlarmStatusJson(std::span<const AlarmEntry> table,
                                     const AlarmClockInfo& clock)
{
    std::ostringstream out;
    out << R"({"time_valid":)" << (clock.timeValid ? "true" : "false")
        << R"(,"local_time":")" << clock.localTime << '"';
    if (clock.next) {
        out << R"(,"next":{"index":)" << clock.next->index
            << R"(,"in_min":)" << clock.next->minutes << '}';
    }
    out << ',';
    writeTable(out, table);
    out << '}';
    return out.str();
}

std::expected<std::vector<AlarmEntry>, ParseError> parseAlarmTableJson(
    std::string_view json)
{
    const std::string_view array = alarmsArray(json);
    if (array.empty()) {
        return std::unexpected(ParseError::MissingField);
    }
    const auto objects = splitObjects(array);
    if (!objects) {
        return std::unexpected(ParseError::InvalidJson);
    }
    std::vector<AlarmEntry> table;
    for (const std::string_view object : *objects) {
        auto entry = parseEntry(object);
        if (!entry) {
            return std::unexpected(ParseError::InvalidJson);
        }
        table.push_back(std::move(*entry));
    }
    return table;
}

std::expected<std::uint16_t, ParseError> parseSleepMinutesJson(
    std::string_view json)
{
    const auto minutes = numberOf(json, "minutes");
    if (!minutes) {
        return std::unexpected(ParseError::MissingField);
    }
    if (!inRange(minutes, kMaxMinutes)) {
        return std::unexpected(ParseError::InvalidJson);
    }
    return static_cast<std::uint16_t>(*minutes);
}

std::string serializeSleepJson(std::uint32_t secondsLeft)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), R"({"active":%s,"seconds_left":%lu})",
                  secondsLeft > 0U ? "true" : "false",
                  static_cast<unsigned long>(secondsLeft));
    return buf;
}

} // namespace core
