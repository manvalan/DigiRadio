/**
 * @file    RadioSpectrum.hpp
 * @brief   RF band scan model, DAB Band III plan and its JSON.
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
 * @date    2026-10-26
 */
#pragma once

#include "core/ParseError.hpp"
#include "core/TunerBand.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace core {

/** FM spectrum sweep start: 87.5 MHz. */
inline constexpr std::uint32_t kSpectrumFmStartKhz = 87500U;
/** FM spectrum sweep end (inclusive): 108.0 MHz. */
inline constexpr std::uint32_t kSpectrumFmStopKhz = 108000U;
/** FM spectrum sweep step: 100 kHz (206 points). */
inline constexpr std::uint32_t kSpectrumFmStepKhz = 100U;

/** DAB Band III channels 5A–13F (ETSI EN 300 401 Table 8 frequencies),
 *  in the order of the tuner's frequency list (index = DAB freq index). */
inline constexpr std::array<std::uint32_t, 38> kDabBand3FrequenciesKhz = {
    174928, 176640, 178352, 180064, 181936, 183648, 185360, 187072, 188928,
    190640, 192352, 194064, 195936, 197648, 199360, 201072, 202928, 204640,
    206352, 208064, 209936, 211648, 213360, 215072, 216928, 218640, 220352,
    222064, 223936, 225648, 227360, 229072, 230784, 232496, 234208, 235776,
    237488, 239200,
};

/** Channel names matching kDabBand3FrequenciesKhz. */
inline constexpr std::array<std::string_view, 38> kDabBand3Channels = {
    "5A",  "5B",  "5C",  "5D",  "6A",  "6B",  "6C",  "6D",  "7A",  "7B",
    "7C",  "7D",  "8A",  "8B",  "8C",  "8D",  "9A",  "9B",  "9C",  "9D",
    "10A", "10B", "10C", "10D", "11A", "11B", "11C", "11D", "12A", "12B",
    "12C", "12D", "13A", "13B", "13C", "13D", "13E", "13F",
};

/** Lifecycle of the (single) spectrum scan. */
enum class SpectrumState : std::uint8_t {
    Idle,    ///< No scan since boot.
    Running, ///< Sweep in progress.
    Done,    ///< Last sweep finished; points are complete.
    Failed,  ///< Last sweep stopped on a tuner error; points are partial.
};

/** Why POST /api/tuner/spectrum could not start a sweep. */
enum class SpectrumStartError : std::uint8_t {
    Busy,       ///< A sweep is already running (HTTP 409).
    NoResource, ///< Task or memory unavailable (HTTP 500).
};

/**
 * @brief    SpectrumPoint — signal measured at one frequency.
 *
 * @dname    SpectrumPoint
 * @return   n/a (type)
 * @pubstate Plain DTO. FM points carry rssi/snr; DAB points carry the
 *           channel index, rssi and, when the ensemble locked, CNR and
 *           FIC quality.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct SpectrumPoint {
    std::uint32_t freqKhz{0U};                ///< Centre frequency.
    std::optional<std::int8_t> rssiDbuV;      ///< RSSI in dBµV.
    std::optional<std::int8_t> snrDb;         ///< FM SNR in dB.
    std::optional<std::uint8_t> dabChannel;   ///< Index into the DAB plan.
    std::optional<std::int8_t> cnrDb;         ///< DAB CNR (locked only).
    std::optional<std::uint8_t> ficQuality;   ///< DAB FIC (locked only).
};

/**
 * @brief    SpectrumSnapshot — state and points of the last sweep.
 *
 * @dname    SpectrumSnapshot
 * @return   n/a (type)
 * @pubstate Plain DTO owned by the scan service, read by the HTTP layer.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct SpectrumSnapshot {
    SpectrumState state{SpectrumState::Idle};  ///< Lifecycle.
    TunerBand band{TunerBand::Fm};             ///< Band swept.
    std::uint8_t progress{0U};                 ///< 0–100 %.
    std::vector<SpectrumPoint> points;         ///< In sweep order.
};

/**
 * @brief    spectrumFmPointCount — number of FM sweep frequencies.
 *
 * @dname    spectrumFmPointCount
 * @return   206 (87.5 to 108.0 MHz inclusive, 100 kHz apart).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] constexpr std::size_t spectrumFmPointCount() noexcept
{
    return (kSpectrumFmStopKhz - kSpectrumFmStartKhz) / kSpectrumFmStepKhz
           + 1U;
}

/**
 * @brief    spectrumProgress — percentage for done of total steps.
 *
 * @dname    spectrumProgress
 * @param    done   Points measured so far.
 * @param    total  Points in the sweep (0 reports 100).
 * @return   0–100, reaching 100 only when done == total.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::uint8_t spectrumProgress(std::size_t done,
                                            std::size_t total) noexcept;

/**
 * @brief    parseSpectrumRequestJson — read {"band":"fm"|"dab"}.
 *
 * @dname    parseSpectrumRequestJson
 * @param    json  POST /api/tuner/spectrum body.
 * @return   Band to sweep, or ParseError::MissingField (no "band") /
 *           InvalidJson (not an object, or band not fm/dab).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::expected<TunerBand, ParseError>
parseSpectrumRequestJson(std::string_view json);

/**
 * @brief    serializeSpectrumJson — body for GET /api/tuner/spectrum.
 *
 * @dname    serializeSpectrumJson
 * @param    snapshot  Current scan state and points.
 * @return   {"state","band","progress","points":[{"freq_khz",
 *           "rssi_dbuv","snr_db"} | DAB {"freq_khz","channel",
 *           "rssi_dbuv","cnr_db","fic_quality"}]} (absent fields omitted).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string serializeSpectrumJson(
    const SpectrumSnapshot& snapshot);

} // namespace core
