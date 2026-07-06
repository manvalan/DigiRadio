/**
 * @file    AudioProfileJson.hpp
 * @brief   JSON parse/serialise for audio profile API (pure core).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/AudioProfile.hpp"
#include "core/ParseError.hpp"

#include "core/EnhanceLevel.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    serializeAudioProfileJson — serialise profile for GET /api/audio.
 *
 * @dname    serializeAudioProfileJson
 * @param    profile  Domain snapshot from AudioService.
 * @return   JSON object string for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeAudioProfileJson(
    const AudioProfile& profile);

/**
 * @brief    parseAudioProfileJson — parse PUT /api/audio/profile body.
 *
 * @dname    parseAudioProfileJson
 * @param    json  Untrusted request body.
 * @return   AudioProfile on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<AudioProfile, ParseError> parseAudioProfileJson(
    std::string_view json);

/**
 * @brief    serializeAudioSavedJson — success response after profile apply.
 *
 * @dname    serializeAudioSavedJson
 * @return   JSON object with \c status set to \c saved.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeAudioSavedJson();

/**
 * @brief    serializeAudioErrorJson — error response for audio routes.
 *
 * @dname    serializeAudioErrorJson
 * @param    reason  Safe token (never includes secrets).
 * @return   JSON object with status and reason fields.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeAudioErrorJson(std::string_view reason);

/**
 * @brief    parseEnhanceLevelJson — parse POST body with level 0..100.
 *
 * @dname    parseEnhanceLevelJson
 * @param    json  Untrusted request body.
 * @return   EnhanceLevel on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<EnhanceLevel, ParseError> parseEnhanceLevelJson(
    std::string_view json);

} // namespace core
