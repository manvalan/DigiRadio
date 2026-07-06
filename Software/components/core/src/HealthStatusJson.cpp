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

namespace core {

namespace {

/**
 * @brief    healthStateToken — map HealthState to the API status string.
 *
 * @dname    healthStateToken
 * @param    state  Health indicator to encode.
 * @return   JSON string token without quotes (e.g. ok).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
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
    return std::string("{\"status\":\"") + healthStateToken(status.state())
           + "\",\"fw\":\"" + std::string(status.firmware().value()) + "\"}";
}

} // namespace core
