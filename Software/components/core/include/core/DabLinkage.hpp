/**
 * @file    DabLinkage.hpp
 * @brief   DAB service-following data (FIG 0/6, 0/24, 0/21) and parsers.
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

#include "core/ParseError.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace core {

/** AN649 0xB7 LINK_TYPE[5:4] (ETSI EN 300 401 clause 8.1.15). */
enum class DabLinkType : std::uint8_t {
    Dab = 0U,    ///< Link ids are DAB/DMB service ids.
    RdsPi = 1U,  ///< Link ids are RDS PI codes (FM).
    AmFm = 2U,   ///< AM/FM (unused per ETSI TS 103 176).
    Drm = 3U,    ///< DRM / AMSS.
};

/** ETSI EN 300 401 clause 8.1.8 R&M codes carried by AN649 0xBF FI_RNM. */
inline constexpr std::uint8_t kFiRnmDabEnsemble = 0x00U;
/** Older EN 300 401 editions: DAB ensemble with local windows. */
inline constexpr std::uint8_t kFiRnmDabEnsembleLocal = 0x01U;
/** FM with RDS: FI id is the RDS PI code. */
inline constexpr std::uint8_t kFiRnmFmRds = 0x08U;
/** AN649 0xBF FI_FREQ_INDEX value for "not in the frequency table". */
inline constexpr std::uint8_t kFiNoFreqIndex = 0xFFU;

/**
 * @brief    DabLinkageSet — one FIG 0/6 linkage set segment.
 *
 * @dname    DabLinkageSet
 * @return   n/a (type)
 * @pubstate Plain DTO from parseDabServiceLinking().
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct DabLinkageSet {
    std::uint16_t lsn;              ///< Linkage set number.
    bool active;                    ///< Link actuated by the broadcaster.
    bool hard;                      ///< Hard link (same programme).
    DabLinkType type;               ///< What the ids are.
    std::vector<std::uint32_t> ids; ///< Linked SIds or PI codes.
};

/**
 * @brief    DabFrequencyInfo — one FIG 0/21 frequency entry.
 *
 * @dname    DabFrequencyInfo
 * @return   n/a (type)
 * @pubstate Plain DTO from parseDabFreqInfo().
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct DabFrequencyInfo {
    std::uint32_t id;          ///< EId (DAB) or PI code (FM with RDS).
    std::uint32_t frequencyHz; ///< Carrier frequency in Hz.
    std::uint8_t freqIndex;    ///< Index in the DAB frequency table, or 0xFF.
    std::uint8_t rnm;          ///< Range & modulation (kFiRnm*).
};

/**
 * @brief    DabLinkage — everything known about where a service also airs.
 *
 * @dname    DabLinkage
 * @return   n/a (type)
 * @pubstate Plain DTO assembled by the tuner from AN649 0xB7/0xC1/0xBF.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
struct DabLinkage {
    std::vector<DabLinkageSet> sets;            ///< FIG 0/6 linkage sets.
    std::vector<std::uint16_t> otherEnsembles;  ///< FIG 0/24 EIds.
    std::vector<DabFrequencyInfo> frequencies;  ///< FIG 0/21 entries.
};

/**
 * @brief    parseDabServiceLinking — decode AN649 0xB7 reply payload.
 *
 * @dname    parseDabServiceLinking
 * @param    resp  Reply bytes from RESP4 (SIZE[7:0]) onwards.
 * @return   Linkage sets, or ParseError::InvalidJson on a truncated reply.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<std::vector<DabLinkageSet>, ParseError>
parseDabServiceLinking(std::span<const std::uint8_t> resp);

/**
 * @brief    parseDabOeServices — decode AN649 0xC1 reply payload.
 *
 * @dname    parseDabOeServices
 * @param    resp  Reply bytes from RESP4 (SIZE[7:0]) onwards.
 * @return   Ensemble ids carrying the service, or InvalidJson if truncated.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<std::vector<std::uint16_t>, ParseError>
parseDabOeServices(std::span<const std::uint8_t> resp);

/**
 * @brief    parseDabFreqInfo — decode AN649 0xBF reply payload.
 *
 * @dname    parseDabFreqInfo
 * @param    resp  Reply bytes from RESP4 (LENGTH_FI_LIST[7:0]) onwards.
 * @return   Frequency entries (12 bytes each). AN649 does not say whether
 *           LENGTH_FI_LIST counts entries or bytes, so both readings are
 *           tried and the one whose entries are all plausible wins.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[nodiscard]] std::expected<std::vector<DabFrequencyInfo>, ParseError>
parseDabFreqInfo(std::span<const std::uint8_t> resp);

} // namespace core
