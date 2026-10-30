/**
 * @file    AfFollowJson.cpp
 * @brief   AfFollowJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */
#include "core/AfFollowJson.hpp"

#include "core/DabReturnPolicy.hpp"

#include <cstddef>

namespace core {

namespace {

constexpr std::string_view kEnabledKey = "\"enabled\":";
constexpr std::string_view kDabReturnKey = "\"dab_return_min\":";

enum class FieldState { Absent, Valid, Invalid };

// Value text after `key`, or nullopt when the key is absent.
[[nodiscard]] std::optional<std::string_view> valueAfter(
    std::string_view json, std::string_view key)
{
    const std::size_t keyPos = json.find(key);
    if (keyPos == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t start =
        json.find_first_not_of(" \t\r\n", keyPos + key.size());
    return start == std::string_view::npos ? std::string_view()
                                           : json.substr(start);
}

[[nodiscard]] FieldState parseEnabled(std::string_view json,
                                      AfFollowSettings& out)
{
    const auto rest = valueAfter(json, kEnabledKey);
    if (!rest) {
        return FieldState::Absent;
    }
    if (rest->starts_with("true") || rest->starts_with("false")) {
        out.enabled = rest->starts_with("true");
        return FieldState::Valid;
    }
    return FieldState::Invalid;
}

[[nodiscard]] FieldState parseDabReturn(std::string_view json,
                                        AfFollowSettings& out)
{
    const auto rest = valueAfter(json, kDabReturnKey);
    if (!rest) {
        return FieldState::Absent;
    }
    std::uint32_t value = 0U;
    std::size_t digits = 0U;
    while (digits < rest->size() && digits < 4U
           && (*rest)[digits] >= '0' && (*rest)[digits] <= '9') {
        value = value * 10U + static_cast<std::uint32_t>((*rest)[digits] - '0');
        ++digits;
    }
    if (digits == 0U || value > kMaxDabReturnMinutes) {
        return FieldState::Invalid;
    }
    out.dabReturnMinutes = static_cast<std::uint16_t>(value);
    return FieldState::Valid;
}

} // namespace

std::expected<AfFollowSettings, ParseError>
parseAfFollowSettingsJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    AfFollowSettings settings;
    const FieldState enabled = parseEnabled(json, settings);
    const FieldState dabReturn = parseDabReturn(json, settings);
    if (enabled == FieldState::Invalid || dabReturn == FieldState::Invalid) {
        return std::unexpected(ParseError::InvalidJson);
    }
    if (enabled == FieldState::Absent && dabReturn == FieldState::Absent) {
        return std::unexpected(ParseError::MissingField);
    }
    return settings;
}

std::string serializeAfFollowJson(const AfFollowStats& stats)
{
    std::string out = std::string(R"({"enabled":)")
                      + (stats.enabled ? "true" : "false")
                      + R"(,"hunts":)" + std::to_string(stats.hunts)
                      + R"(,"switches":)" + std::to_string(stats.switches);
    if (stats.lastFrom && stats.lastTo) {
        out += R"(,"last_from_khz":)" + std::to_string(stats.lastFrom->value())
               + R"(,"last_to_khz":)" + std::to_string(stats.lastTo->value());
    }
    out += R"(,"dab_hunts":)" + std::to_string(stats.dabHunts)
           + R"(,"dab_switches":)" + std::to_string(stats.dabSwitches)
           + R"(,"dab_return_min":)" + std::to_string(stats.dabReturnMinutes)
           + R"(,"on_fm_fallback":)" + (stats.onFmFallback ? "true" : "false")
           + R"(,"dab_returns":)" + std::to_string(stats.dabReturns);
    return out + "}";
}

} // namespace core
