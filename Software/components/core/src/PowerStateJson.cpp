/**
 * @file    PowerStateJson.cpp
 * @brief   JSON parse/serialise for GET/POST /api/system/power.
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
 * @date    2026-10-21
 */
#include "core/PowerStateJson.hpp"

#include <cstddef>

namespace core {

namespace {

constexpr std::string_view kStateKey = "\"state\":";
constexpr std::string_view kOn = "on";
constexpr std::string_view kStandby = "standby";

[[nodiscard]] std::string_view extractStateValue(std::string_view json)
{
    const std::size_t keyPos = json.find(kStateKey);
    if (keyPos == std::string_view::npos) {
        return {};
    }
    const std::size_t quote = json.find_first_not_of(
        " \t\r\n", keyPos + kStateKey.size());
    if (quote == std::string_view::npos || json[quote] != '"') {
        return {};
    }
    const std::size_t end = json.find('"', quote + 1U);
    if (end == std::string_view::npos) {
        return {};
    }
    return json.substr(quote + 1U, end - quote - 1U);
}

} // namespace

std::expected<PowerState, ParseError> parsePowerStateJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string_view value = extractStateValue(json);
    if (value.empty()) {
        return std::unexpected(ParseError::MissingField);
    }
    if (value == kOn) {
        return PowerState::On;
    }
    if (value == kStandby) {
        return PowerState::Standby;
    }
    return std::unexpected(ParseError::InvalidJson);
}

std::string serializePowerStateJson(PowerState state)
{
    const std::string_view token =
        state == PowerState::Standby ? kStandby : kOn;
    return std::string(R"({"state":")") + std::string(token) + R"("})";
}

} // namespace core
