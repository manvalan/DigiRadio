/**
 * @file    SignalQuality.hpp
 * @brief   Reception level and a 0–100 goodness score for FM and DAB.
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

#include "core/TunerStatus.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    SignalQuality — reception summary for GET /api/tuner/signal.
 *
 * @dname    SignalQuality
 * @return   n/a (type)
 * @pubstate Plain DTO built by computeSignalQuality().
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct SignalQuality {
    TunerBand band;                     ///< Band the figures refer to.
    bool locked;                        ///< Valid lock (FM VALID / DAB).
    std::optional<std::int8_t> levelDbuV; ///< RSSI in dBµV.
    std::optional<std::int8_t> snrDb;   ///< FM SNR or DAB CNR in dB.
    std::optional<std::uint8_t> ficQuality; ///< DAB FIC quality 0–100.
    std::uint8_t quality;               ///< Goodness 0 (none) – 100.
};

/**
 * @brief    computeSignalQuality — score the current reception.
 *
 * @dname    computeSignalQuality
 * @param    status  Latest tuner snapshot.
 * @return   FM: 60% SNR (3→25 dB) + 40% RSSI (5→45 dBµV), capped at 20
 *           without VALID. DAB: 70% FIC quality + 30% CNR (5→25 dB), 0
 *           without lock.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] SignalQuality computeSignalQuality(const TunerStatus& status);

/**
 * @brief    signalGradeToken — word for a quality score.
 *
 * @dname    signalGradeToken
 * @param    quality  Score 0–100.
 * @return   "excellent" (≥75), "good" (≥50), "fair" (≥25), "poor" (>0)
 *           or "none".
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::string_view signalGradeToken(std::uint8_t quality) noexcept;

/**
 * @brief    serializeSignalQualityJson — body for GET /api/tuner/signal.
 *
 * @dname    serializeSignalQualityJson
 * @param    signal  Computed reception summary.
 * @return   {"band","locked","level_dbuv","snr_db","fic_quality",
 *           "quality","grade"}; unknown figures are omitted.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::string serializeSignalQualityJson(const SignalQuality& signal);

} // namespace core
