/**
 * @file    AfFollowJson.hpp
 * @brief   JSON for GET|POST /api/tuner/af (RDS alternate-frequency follow).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */
#pragma once

#include "core/AlternateFrequencyFollower.hpp"
#include "core/ParseError.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    parseAfFollowEnabledJson — parse POST body {"enabled":bool}.
 *
 * @dname    parseAfFollowEnabledJson
 * @param    json  Untrusted request body.
 * @return   The requested flag; ParseError::MissingField without an
 *           "enabled" key, InvalidJson for a non-object body or a value
 *           other than true / false.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<bool, ParseError> parseAfFollowEnabledJson(
    std::string_view json);

/**
 * @brief    serializeAfFollowJson — response body for /api/tuner/af.
 *
 * @dname    serializeAfFollowJson
 * @param    stats  Current AF-follow state.
 * @return   {"enabled":bool,"hunts":n,"switches":n} plus "last_from_khz" and
 *           "last_to_khz" once a switch happened.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::string serializeAfFollowJson(const AfFollowStats& stats);

} // namespace core
