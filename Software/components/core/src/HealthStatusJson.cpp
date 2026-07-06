/**
 * @file    HealthStatusJson.cpp
 * @brief   JSON serialisation for HealthStatus.
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
 * @date    2026-07-06
 */

#include "core/HealthStatusJson.hpp"

#include <optional>
#include <string>

namespace core {

namespace {

[[nodiscard]] const char* healthStateToken(HealthState state) noexcept
{
    switch (state) {
    case HealthState::Ok:
        return "ok";
    }
    return "unknown";
}

} // namespace

std::string serializeHealthStatusJson(const HealthStatus& status)
{
    std::string json = std::string("{\"status\":\"") + healthStateToken(status.state())
                       + "\",\"fw\":\"" + std::string(status.firmware().value()) + "\"";
    if (const std::optional<CompanionChipStatus>& chips = status.chips(); chips) {
        json += std::string(",\"chips\":{")
                + "\"si4684\":" + (chips->si4684Ready ? "true" : "false")
                + ",\"adau1701\":" + (chips->adau1701Ready ? "true" : "false")
                + ",\"bt1035\":" + (chips->bt1035Ready ? "true" : "false") + "}";
    }
    json += '}';
    return json;
}

} // namespace core
