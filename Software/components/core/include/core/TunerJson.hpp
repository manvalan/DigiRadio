/**
 * @file    TunerJson.hpp
 * @brief   JSON parse/serialise for tuner API (pure core).
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

#include "core/FrequencyKHz.hpp"
#include "core/ParseError.hpp"
#include "core/TunerBand.hpp"
#include "core/TunerStatus.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

/**
 * @brief    TunerTuneRequest — parsed POST /api/tuner/tune body.
 *
 * @dname    TunerTuneRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseTunerTuneJson at the HTTP boundary.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct TunerTuneRequest {
    TunerBand band;              ///< Target band (Dab or Fm).
    std::uint8_t dabFreqIndex;   ///< Band III ensemble index (0–37) when band is Dab.
    std::optional<FrequencyKHz> fmFrequency; ///< FM centre frequency when band is Fm.
};

/**
 * @brief    TunerPlayRequest — parsed POST /api/tuner/play body.
 *
 * @dname    TunerPlayRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseTunerPlayJson at the HTTP boundary.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct TunerPlayRequest {
    std::uint32_t serviceId;    ///< DAB service identifier from the ensemble list.
    std::uint32_t componentId;  ///< Audio component within the service.
};

/**
 * @brief    serializeTunerStatusJson — serialise a tuner snapshot for GET status.
 *
 * @dname    serializeTunerStatusJson
 * @param    status  Domain snapshot from TunerService::refreshStatus().
 * @return   JSON object string for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeTunerStatusJson(const TunerStatus& status);

/**
 * @brief    serializeTunerServicesJson — serialise the DAB service list.
 *
 * @dname    serializeTunerServicesJson
 * @param    services  Programme entries for the current ensemble.
 * @return   JSON object string with a services array.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeTunerServicesJson(
    const std::vector<TunerServiceEntry>& services);

/**
 * @brief    serializeTunerErrorJson — serialise a tuner API error response.
 *
 * @dname    serializeTunerErrorJson
 * @param    reason  Short machine-readable cause (never a secret).
 * @return   JSON object string with status error and reason fields.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeTunerErrorJson(const char* reason);

/**
 * @brief    parseTunerTuneJson — validate POST /api/tuner/tune body.
 *
 * @dname    parseTunerTuneJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   TunerTuneRequest on success, or a ParseError.
 * @pubstate none
 *
 * Rejects malformed JSON, missing fields, and out-of-range frequencies
 * before any driver call.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<TunerTuneRequest, ParseError> parseTunerTuneJson(
    std::string_view json);

/**
 * @brief    parseTunerPlayJson — validate POST /api/tuner/play body.
 *
 * @dname    parseTunerPlayJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   TunerPlayRequest on success, or a ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<TunerPlayRequest, ParseError> parseTunerPlayJson(
    std::string_view json);

} // namespace core
