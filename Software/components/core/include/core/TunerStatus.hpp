/**
 * @file    TunerStatus.hpp
 * @brief   Tuner status DTO for API and UI (pure core).
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
#include "core/TunerBand.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace core {

/**
 * @brief    TunerServiceEntry — one DAB programme in the current ensemble.
 *
 * @dname    TunerServiceEntry
 * @return   n/a (type)
 * @pubstate Plain DTO returned by ITuner::listDabServices().
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct TunerServiceEntry {
    std::uint32_t serviceId;    ///< DAB service identifier.
    std::uint32_t componentId;  ///< Audio component within the service.
    std::array<char, 17> label; ///< UTF-8 programme label, NUL-terminated.
};

/**
 * @brief    TunerStatus — snapshot for GET /api/tuner/status.
 *
 * @dname    TunerStatus
 * @return   n/a (type)
 * @pubstate Plain DTO built by ITuner::readStatus(); serialised by
 *           serializeTunerStatusJson(). Band-specific fields are optional.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct TunerStatus {
    bool booted;                              ///< Companion tuner boot completed.
    TunerBand band;                           ///< Active loaded application band.
    bool locked;                              ///< RF lock / valid signal.
    std::uint8_t volume;                      ///< Attenuator level 0–63.
    std::optional<std::uint8_t> dabFreqIndex; ///< Current Band III ensemble index.
    std::optional<std::uint8_t> dabFicQuality; ///< FIC quality 0–100 when DAB.
    std::optional<std::int8_t> dabCnrDb;    ///< CNR in dB when DAB.
    std::optional<FrequencyKHz> fmFrequency; ///< Tuned FM centre frequency.
    std::optional<std::int8_t> fmRssiDbuV;  ///< FM RSSI in dBµV.
    std::optional<std::int8_t> fmSnrDb;     ///< FM SNR in dB.
    std::optional<bool> fmStereo;           ///< FM stereo pilot detected.
};

} // namespace core
