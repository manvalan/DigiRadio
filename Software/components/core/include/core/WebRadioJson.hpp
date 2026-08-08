/**
 * @file    WebRadioJson.hpp
 * @brief   JSON parse/serialise for the web radio streaming API (pure core).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-08
 */
#pragma once

#include "core/ParseError.hpp"
#include "core/WebRadioConfig.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    parseWebRadioConfigJson — parse POST /api/streaming body.
 *
 * @dname    parseWebRadioConfigJson
 * @param    json  Untrusted request body: {"enabled":bool,"url":string}.
 * @return   WebRadioConfig on success, or ParseError. Rejects URLs that
 *           are missing, longer than 200 bytes, or not http://.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-08
 */
[[nodiscard]] std::expected<WebRadioConfig, ParseError> parseWebRadioConfigJson(
    std::string_view json);

/**
 * @brief    serializeWebRadioConfigJson — serialise for GET /api/streaming.
 *
 * @dname    serializeWebRadioConfigJson
 * @param    config  Current streaming configuration.
 * @return   JSON object string for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-08
 */
[[nodiscard]] std::string serializeWebRadioConfigJson(
    const WebRadioConfig& config);

/**
 * @brief    serializeWebRadioErrorJson — error response for streaming routes.
 *
 * @dname    serializeWebRadioErrorJson
 * @param    reason  Safe token (never includes secrets).
 * @return   JSON object with status and reason fields.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-08
 */
[[nodiscard]] std::string serializeWebRadioErrorJson(std::string_view reason);

} // namespace core
