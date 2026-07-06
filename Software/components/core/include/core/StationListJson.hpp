/**
 * @file    StationListJson.hpp
 * @brief   JSON parse/serialise for station preset list (pure core).
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

#include "core/ParseError.hpp"
#include "core/Station.hpp"
#include "core/StationList.hpp"
#include "core/StationListError.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    StationRemoveRequest — parsed POST /api/stations/remove body.
 *
 * @dname    StationRemoveRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseStationRemoveJson.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct StationRemoveRequest {
    std::size_t index; ///< Zero-based list index to delete.
};

/**
 * @brief    serializeStationListJson — serialise all presets for GET /api/stations.
 *
 * @dname    serializeStationListJson
 * @param    list  Domain preset collection.
 * @return   JSON object with a stations array.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeStationListJson(const StationList& list);

/**
 * @brief    parseStationListJson — load presets from persisted NVS JSON.
 *
 * @dname    parseStationListJson
 * @param    json  Untrusted blob from secure storage.
 * @return   StationList on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<StationList, ParseError>
parseStationListJson(std::string_view json);

/**
 * @brief    parseStationJson — validate one POST /api/stations body.
 *
 * @dname    parseStationJson
 * @param    json  Untrusted request body.
 * @return   Station on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<Station, ParseError> parseStationJson(
    std::string_view json);

/**
 * @brief    parseStationRemoveJson — validate POST /api/stations/remove body.
 *
 * @dname    parseStationRemoveJson
 * @param    json  Untrusted request body with index field.
 * @return   StationRemoveRequest on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<StationRemoveRequest, ParseError>
parseStationRemoveJson(std::string_view json);

/**
 * @brief    serializeStationListErrorJson — serialise a station API error.
 *
 * @dname    serializeStationListErrorJson
 * @param    reason  Short machine-readable cause.
 * @return   JSON error object.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeStationListErrorJson(const char* reason);

/**
 * @brief    stationListErrorToken — map domain error to API reason string.
 *
 * @dname    stationListErrorToken
 * @param    error  StationListError from add/remove/move.
 * @return   Stable reason token.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] const char* stationListErrorToken(StationListError error) noexcept;

} // namespace core
