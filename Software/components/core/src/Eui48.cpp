/**
 * @file    Eui48.cpp
 * @brief   Eui48 implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/Eui48.hpp"

#include <iomanip>
#include <sstream>

namespace core {

namespace {

[[nodiscard]] std::string toUpperHex(std::string_view bytes)
{
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setfill('0');
    for (const unsigned char byte : bytes) {
        out << std::setw(2) << static_cast<unsigned>(byte);
    }
    return out.str();
}

} // namespace

Eui48 Eui48::fromBytes(const std::array<std::uint8_t, 6>& bytes) noexcept
{
    return Eui48(bytes);
}

Eui48::Eui48(std::array<std::uint8_t, 6> bytes) noexcept
    : bytes_(bytes)
{
}

const std::array<std::uint8_t, 6>& Eui48::bytes() const noexcept
{
    return bytes_;
}

std::string Eui48::serialNumber() const
{
    return toUpperHex(
        std::string_view(reinterpret_cast<const char*>(bytes_.data()),
                         bytes_.size()));
}

std::string Eui48::shortSuffix() const
{
    return toUpperHex(
        std::string_view(reinterpret_cast<const char*>(bytes_.data() + 3U), 3U));
}

} // namespace core
