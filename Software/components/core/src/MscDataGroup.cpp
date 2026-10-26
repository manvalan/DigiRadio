/**
 * @file    MscDataGroup.cpp
 * @brief   MSC data group parser (ETSI EN 300 401 §5.3.3).
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

#include "core/MscDataGroup.hpp"

namespace core {

namespace {

constexpr std::uint8_t kExtensionFlag = 0x80U;
constexpr std::uint8_t kCrcFlag = 0x40U;
constexpr std::uint8_t kSegmentFlag = 0x20U;
constexpr std::uint8_t kUserAccessFlag = 0x10U;
constexpr std::uint8_t kTransportIdFlag = 0x10U;

[[nodiscard]] std::uint16_t be16(std::span<const std::uint8_t> b,
                                 std::size_t at) noexcept
{
    return static_cast<std::uint16_t>((b[at] << 8U) | b[at + 1U]);
}

// Session header user-access field (§5.3.3.2): Rfa[7:5], Transport Id
// flag[4], Length indicator[3:0] = bytes that follow (TID + address).
[[nodiscard]] bool parseUserAccess(std::span<const std::uint8_t> b,
                                   std::size_t& pos, MscDataGroup& out)
{
    if (pos >= b.size()) {
        return false;
    }
    const std::uint8_t field = b[pos++];
    const std::size_t length = field & 0x0FU;
    if (pos + length > b.size()) {
        return false;
    }
    if ((field & kTransportIdFlag) != 0U) {
        if (length < 2U) {
            return false;
        }
        out.transportId = be16(b, pos);
    }
    pos += length;
    return true;
}

[[nodiscard]] bool parseSession(std::span<const std::uint8_t> b,
                                std::uint8_t flags, std::size_t& pos,
                                MscDataGroup& out)
{
    if ((flags & kSegmentFlag) != 0U) {
        if (pos + 2U > b.size()) {
            return false;
        }
        const std::uint16_t field = be16(b, pos);
        out.last = (field & 0x8000U) != 0U;
        out.segment = static_cast<std::uint16_t>(field & 0x7FFFU);
        pos += 2U;
    }
    return (flags & kUserAccessFlag) == 0U || parseUserAccess(b, pos, out);
}

} // namespace

std::uint16_t crc16Ccitt(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint16_t crc = 0xFFFFU;
    for (const std::uint8_t byte : bytes) {
        crc = static_cast<std::uint16_t>(crc ^ (byte << 8U));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) != 0U
                      ? static_cast<std::uint16_t>((crc << 1U) ^ 0x1021U)
                      : static_cast<std::uint16_t>(crc << 1U);
        }
    }
    return static_cast<std::uint16_t>(~crc);
}

std::expected<MscDataGroup, ParseError> parseMscDataGroup(
    std::span<const std::uint8_t> bytes)
{
    if (bytes.size() < 2U) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::uint8_t flags = bytes[0];
    std::span<const std::uint8_t> body = bytes;
    if ((flags & kCrcFlag) != 0U) {
        if (bytes.size() < 4U
            || crc16Ccitt(bytes.first(bytes.size() - 2U))
                   != be16(bytes, bytes.size() - 2U)) {
            return std::unexpected(ParseError::InvalidJson);
        }
        body = bytes.first(bytes.size() - 2U);
    }
    MscDataGroup group;
    group.type = flags & 0x0FU;
    std::size_t pos = (flags & kExtensionFlag) != 0U ? 4U : 2U;
    if (pos > body.size() || !parseSession(body, flags, pos, group)) {
        return std::unexpected(ParseError::InvalidJson);
    }
    group.data = body.subspan(pos);
    return group;
}

} // namespace core
