/**
 * @file    WifiProvisionJson.hpp
 * @brief   Parse and serialise Wi-Fi provisioning JSON at the boundary.
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
#pragma once

#include "core/ParseError.hpp"
#include "core/WifiCredentials.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    parseWifiProvisionJson — validate POST body into credentials.
 *
 * @dname    parseWifiProvisionJson
 * @param    json  Untrusted request body, e.g.
 *                 {"ssid":"MyNet","password":"secret"}.
 * @return   WifiCredentials on success, or a ParseError.
 * @pubstate none
 *
 * Minimal parser for the Slice 2 provisioning endpoint; rejects malformed
 * input before any persistence or Wi-Fi driver calls.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<WifiCredentials, ParseError>
parseWifiProvisionJson(std::string_view json);

/**
 * @brief    serializeWifiProvisionSavedJson — success response body.
 *
 * @dname    serializeWifiProvisionSavedJson
 * @param    rebootInSec  Seconds until the device reboots into STA mode.
 * @return   JSON object string.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string
serializeWifiProvisionSavedJson(unsigned rebootInSec);

/**
 * @brief    serializeWifiProvisionErrorJson — rejection response body.
 *
 * @dname    serializeWifiProvisionErrorJson
 * @param    reason  Short machine-readable cause (never a secret).
 * @return   JSON object string.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string
serializeWifiProvisionErrorJson(std::string_view reason);

} // namespace core
