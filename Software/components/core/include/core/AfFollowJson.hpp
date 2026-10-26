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

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    AfFollowSettings — fields of a POST /api/tuner/af body.
 *
 * @dname    AfFollowSettings
 * @return   n/a (type)
 * @pubstate Plain DTO; each field set only when present in the body.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct AfFollowSettings {
    std::optional<bool> enabled;                ///< "enabled".
    std::optional<std::uint16_t> dabReturnMinutes; ///< "dab_return_min".
};

/**
 * @brief    parseAfFollowSettingsJson — parse POST body
 *           {"enabled":bool,"dab_return_min":0-120}, each key optional.
 *
 * @dname    parseAfFollowSettingsJson
 * @param    json  Untrusted request body.
 * @return   The requested settings; ParseError::MissingField when neither
 *           key is present, InvalidJson for a non-object body, a
 *           non-boolean "enabled" or a "dab_return_min" outside 0–120.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<AfFollowSettings, ParseError>
parseAfFollowSettingsJson(std::string_view json);

/**
 * @brief    serializeAfFollowJson — response body for /api/tuner/af.
 *
 * @dname    serializeAfFollowJson
 * @param    stats  Current AF-follow state.
 * @return   {"enabled":bool,"hunts":n,"switches":n} plus "last_from_khz" and
 *           "last_to_khz" once a switch happened, then "dab_hunts",
 *           "dab_switches", "dab_return_min", "on_fm_fallback" and
 *           "dab_returns" (DAB service following and the FM→DAB return).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::string serializeAfFollowJson(const AfFollowStats& stats);

} // namespace core
