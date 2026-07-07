/**
 * @file    BluetoothJson.cpp
 * @brief   BluetoothJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BluetoothJson.hpp"

#include <cstdlib>
#include <sstream>

namespace core {

namespace {

void appendJsonString(std::ostringstream& out, std::string_view text)
{
    out << '"';
    for (const char ch : text) {
        if (ch == '"' || ch == '\\') {
            out << '\\';
        }
        out << ch;
    }
    out << '"';
}

} // namespace

std::string serializeBluetoothStatusJson(const BluetoothStatus& status)
{
    std::ostringstream out;
    out << "{\"booted\":" << (status.booted ? "true" : "false")
        << ",\"pairing\":" << (status.pairing ? "true" : "false")
        << ",\"a2dp\":\"" << a2dpStateToken(status.a2dpState) << "\""
        << ",\"device_name\":";
    appendJsonString(out, status.deviceName);
    out << ",\"auto_reconnect\":"
        << static_cast<unsigned>(status.autoReconnect) << "}";
    return out.str();
}

std::string serializeBluetoothErrorJson(const char* reason)
{
    std::ostringstream out;
    out << "{\"status\":\"error\",\"reason\":\"" << reason << "\"}";
    return out.str();
}

std::string serializeBluetoothPairedJson(
    const std::vector<Bt1035PairedDevice>& devices)
{
    std::ostringstream out;
    out << "{\"devices\":[";
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const Bt1035PairedDevice& device = devices[i];
        out << "{\"index\":" << static_cast<unsigned>(device.index)
            << ",\"mac\":";
        appendJsonString(out, device.mac);
        out << ",\"name\":";
        appendJsonString(out, device.name);
        out << "}";
    }
    out << "]}";
    return out.str();
}

std::expected<std::uint8_t, ParseError>
parseBluetoothAutoReconnectJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string needle = "\"times\":";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    char* end = nullptr;
    const unsigned long raw =
        std::strtoul(json.data() + start + needle.size(), &end, 10);
    if (end == json.data() + start + needle.size() || raw > 15U) {
        return std::unexpected(ParseError::InvalidJson);
    }
    return static_cast<std::uint8_t>(raw);
}

} // namespace core
