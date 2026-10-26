/**
 * @file    MscDataGroup.hpp
 * @brief   Parse DAB MSC data groups (ETSI EN 300 401 §5.3.3).
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

#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace core {

/** Data group types used by MOT (EN 300 401 §5.3.3.1, EN 301 234 §5.1). */
inline constexpr std::uint8_t kDataGroupMotHeader = 3U;
/** MOT body, unscrambled. */
inline constexpr std::uint8_t kDataGroupMotBody = 4U;

/**
 * @brief    MscDataGroup — the fields of one data group a MOT decoder needs.
 *
 * @dname    MscDataGroup
 * @return   n/a (type)
 * @pubstate Plain DTO; data borrows the parsed buffer.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct MscDataGroup {
    std::uint8_t type{0U};                    ///< Data group type (4 bits).
    std::optional<std::uint16_t> segment;     ///< Segment number (15 bits).
    bool last{false};                         ///< Last segment flag.
    std::optional<std::uint16_t> transportId; ///< MOT transport id.
    std::span<const std::uint8_t> data;       ///< Data group data field.
};

/**
 * @brief    parseMscDataGroup — split a data group into header and data.
 *
 * @dname    parseMscDataGroup
 * @param    bytes  One complete data group, CRC included when flagged.
 * @return   The parsed group, or ParseError::InvalidJson when it is
 *           truncated or its CRC (CRC-16 CCITT, §5.3.3.4) does not match.
 * @pubstate none
 *
 * Header byte 0: extension, CRC, segment and user-access flags plus the
 * type; byte 1: continuity/repetition. Then the optional 2-byte
 * extension field, the session header (2-byte Last + segment number;
 * user access with transport id and end-user address) and the data.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::expected<MscDataGroup, ParseError> parseMscDataGroup(
    std::span<const std::uint8_t> bytes);

/**
 * @brief    crc16Ccitt — DAB data group CRC (EN 300 401 §5.3.3.4).
 *
 * @dname    crc16Ccitt
 * @param    bytes  Covered bytes.
 * @return   x^16+x^12+x^5+1, preset 0xFFFF, ones' complement result.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::uint16_t crc16Ccitt(
    std::span<const std::uint8_t> bytes) noexcept;

} // namespace core
