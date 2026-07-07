/**
 * @file    Eui48.hpp
 * @brief   Strong type for a factory-programmed 48-bit EUI from 24AA025E48.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    Eui48 — immutable six-byte globally unique identifier.
 *
 * @dname    Eui48
 * @return   n/a (type)
 * @pubstate Owns bytes_; constructed only from a full six-byte payload.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class Eui48 {
public:
    /**
     * @brief    fromBytes — wrap a factory EUI-48 read from EEPROM.
     *
     * @dname    fromBytes
     * @param    bytes  Six-byte EUI-48 in datasheet order (0xFA..0xFF).
     * @return   Eui48 value.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] static Eui48 fromBytes(
        const std::array<std::uint8_t, 6>& bytes) noexcept;

    /**
     * @brief    bytes — read the raw EUI-48 octets.
     *
     * @dname    bytes
     * @return   Reference to the six stored bytes.
     * @pubstate reads bytes_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] const std::array<std::uint8_t, 6>& bytes() const noexcept;

    /**
     * @brief    serialNumber — canonical uppercase hex serial (12 digits).
     *
     * @dname    serialNumber
     * @return   Serial string without separators (e.g. AABBCCDDEEFF).
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string serialNumber() const;

    /**
     * @brief    shortSuffix — last three bytes as six uppercase hex digits.
     *
     * @dname    shortSuffix
     * @return   Suffix for SSID, Bluetooth name, and hostname.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::string shortSuffix() const;

private:
    explicit Eui48(std::array<std::uint8_t, 6> bytes) noexcept;

    std::array<std::uint8_t, 6> bytes_;
};

} // namespace core
