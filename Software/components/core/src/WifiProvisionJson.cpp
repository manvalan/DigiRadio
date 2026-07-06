/**
 * @file    WifiProvisionJson.cpp
 * @brief   Wi-Fi provisioning JSON parse/serialise implementation.
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

#include "core/WifiProvisionJson.hpp"

namespace core {

namespace {

/**
 * @brief    extractJsonString — read a quoted string value for a key.
 *
 * @dname    extractJsonString
 * @param    json  Full JSON object text.
 * @param    key   Field name without quotes (e.g. ssid).
 * @return   Decoded string view into json, or empty on failure.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string_view extractJsonString(std::string_view json,
                                                 std::string_view key)
{
    const std::string needle = std::string("\"") + std::string(key) + "\":\"";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return {};
    }
    const std::size_t valueStart = start + needle.size();
    const std::size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return {};
    }
    return json.substr(valueStart, valueEnd - valueStart);
}

} // namespace

std::expected<WifiCredentials, ParseError>
parseWifiProvisionJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    const std::string_view ssidRaw = extractJsonString(json, "ssid");
    if (ssidRaw.empty() && json.find("\"ssid\"") == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    if (!WifiSsid::isValid(ssidRaw)) {
        return std::unexpected(ParseError::InvalidSsid);
    }

    std::string_view passwordRaw;
    if (json.find("\"password\"") != std::string_view::npos) {
        passwordRaw = extractJsonString(json, "password");
    }
    if (!WifiCredentials::isPasswordValid(passwordRaw)) {
        return std::unexpected(ParseError::InvalidPassword);
    }

    return WifiCredentials(WifiSsid(ssidRaw), Secret(std::string(passwordRaw)));
}

std::string serializeWifiProvisionSavedJson(unsigned rebootInSec)
{
    return std::string("{\"status\":\"saved\",\"reboot_in_sec\":")
           + std::to_string(rebootInSec) + "}";
}

std::string serializeWifiProvisionErrorJson(std::string_view reason)
{
    return std::string("{\"status\":\"error\",\"reason\":\"") + std::string(reason)
           + "\"}";
}

} // namespace core
