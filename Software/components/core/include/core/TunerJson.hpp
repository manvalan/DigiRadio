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
#include "core/SeekDirection.hpp"
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
    std::optional<std::uint8_t> antCap; ///< Antenna varactor override (0–128),
                                        ///< for calibration sweeps; applies to
                                        ///< whichever band is being tuned.
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
 * @brief    TunerScanRequest — parsed POST /api/tuner/scan body.
 *
 * @dname    TunerScanRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseTunerScanJson at the HTTP boundary.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct TunerScanRequest {
    TunerBand band;           ///< FM seek scan or DAB ensemble scan.
    std::uint8_t maxSteps;    ///< Max FM seeks or DAB ensemble indices to try.
    std::string nameFilter;   ///< Optional case-insensitive substring (PS/DAB label).
};

/**
 * @brief    TunerScanResult — outcome of an automatic station search.
 *
 * @dname    TunerScanResult
 * @return   n/a (type)
 * @pubstate Built by TunerService::scanForStation().
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct TunerScanResult {
    bool found;               ///< True when a station matched criteria.
    TunerBand band;           ///< Band that was scanned.
    std::uint16_t stepsTried; ///< Seek or ensemble attempts performed.
    std::optional<FrequencyKHz> fmFrequency; ///< FM centre when found on FM.
    std::optional<std::uint8_t> dabFreqIndex; ///< Ensemble index when found on DAB.
    std::optional<std::uint32_t> dabServiceId;    ///< Started DAB service id.
    std::optional<std::uint32_t> dabComponentId;  ///< Started DAB component id.
    std::optional<BroadcastLabel> stationName;    ///< RDS PS or DAB label when known.
};

/**
 * @brief    TunerFmScannedStation — one hit from a full FM band scan.
 *
 * @dname    TunerFmScannedStation
 * @return   n/a (type)
 * @pubstate Plain DTO built by TunerService::scanFullFmBand().
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
struct TunerFmScannedStation {
    FrequencyKHz frequency;              ///< Centre frequency of the hit.
    std::int8_t rssiDbuV;                ///< RSSI at the time of the hit.
    std::int8_t snrDb;                   ///< SNR at the time of the hit.
    std::optional<BroadcastLabel> stationName; ///< RDS PS name, if decoded in time.
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

/**
 * @brief    parseTunerSeekJson — validate POST /api/tuner/seek body.
 *
 * @dname    parseTunerSeekJson
 * @param    json  Optional body; empty defaults to seek up.
 * @return   SeekDirection on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<SeekDirection, ParseError> parseTunerSeekJson(
    std::string_view json);

/**
 * @brief    parseTunerScanJson — validate POST /api/tuner/scan body.
 *
 * @dname    parseTunerScanJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   TunerScanRequest on success, or a ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::expected<TunerScanRequest, ParseError> parseTunerScanJson(
    std::string_view json);

/**
 * @brief    serializeTunerScanJson — serialise automatic scan outcome.
 *
 * @dname    serializeTunerScanJson
 * @param    result  Scan result from TunerService::scanForStation().
 * @return   JSON object for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string serializeTunerScanJson(const TunerScanResult& result);

/**
 * @brief    serializeTunerFmBandScanJson — serialise a full FM band scan.
 *
 * @dname    serializeTunerFmBandScanJson
 * @param    stations  Hits from TunerService::scanFullFmBand(), in the
 *                      order the sweep found them (ascending frequency).
 * @return   JSON object with a `stations` array for the HTTP response body.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] std::string serializeTunerFmBandScanJson(
    const std::vector<TunerFmScannedStation>& stations);

/**
 * @brief    AntennaCalibrationRequest — parsed POST
 *           /api/tuner/calibrate-antenna body.
 *
 * @dname    AntennaCalibrationRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseAntennaCalibrationJson at the HTTP
 *           boundary.
 *
 * @author   Michele Bigi
 * @date     2026-08-20
 */
struct AntennaCalibrationRequest {
    TunerBand band;        ///< Which band's ANTCAP default this saves.
    std::uint8_t antCap;   ///< Value to persist (0-128).
};

/**
 * @brief    parseAntennaCalibrationJson — validate POST
 *           /api/tuner/calibrate-antenna body.
 *
 * @dname    parseAntennaCalibrationJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   Band + ANTCAP value (0-128) on success, or a ParseError.
 *           `band` defaults to Fm when the field is omitted, preserving
 *           the original FM-only request shape.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-19
 */
[[nodiscard]] std::expected<AntennaCalibrationRequest, ParseError>
parseAntennaCalibrationJson(std::string_view json);

/**
 * @brief    XtalCalibrationRequest — parsed POST
 *           /api/tuner/xtal-calibrate body.
 *
 * @dname    XtalCalibrationRequest
 * @return   n/a (type)
 * @pubstate Plain DTO filled by parseXtalCalibrationJson at the HTTP
 *           boundary. Diagnostic-only: live Si4684 crystal parameter
 *           recalibration, no ESP32 restart required.
 *
 * @author   Michele Bigi
 * @date     2026-08-23
 */
struct XtalCalibrationRequest {
    std::uint8_t ibias;        ///< POWER_UP ARG3 IBIAS (0-127).
    std::uint8_t ctun;         ///< POWER_UP ARG8 CTUN (0-63).
    std::uint32_t xtalFreqHz;  ///< POWER_UP ARG4-7 XTAL_FREQ in Hz.
};

/**
 * @brief    parseXtalCalibrationJson — validate POST
 *           /api/tuner/xtal-calibrate body.
 *
 * @dname    parseXtalCalibrationJson
 * @param    json  Untrusted request body from the HTTP handler.
 * @return   Crystal parameters on success, or a ParseError. `ibias` and
 *           `ctun` default to the values already loaded at boot when
 *           omitted (unusual to omit, but harmless); `xtal_freq_hz`
 *           defaults to the nominal 19,200,000 Hz.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-23
 */
[[nodiscard]] std::expected<XtalCalibrationRequest, ParseError>
parseXtalCalibrationJson(std::string_view json);

} // namespace core
