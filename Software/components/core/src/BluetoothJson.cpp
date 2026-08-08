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
#include <cctype>
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

std::string serializeBluetoothScanJson(
    const std::vector<Bt1035ScannedDevice>& devices)
{
    std::ostringstream out;
    out << "{\"devices\":[";
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const Bt1035ScannedDevice& device = devices[i];
        out << "{\"index\":" << static_cast<unsigned>(device.index)
            << ",\"mac\":";
        appendJsonString(out, device.mac);
        out << ",\"name\":";
        appendJsonString(out, device.name);
        out << ",\"rssi_dbm\":" << device.rssiDbm << "}";
    }
    out << "]}";
    return out.str();
}

std::expected<std::string, ParseError>
parseBluetoothConnectJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string needle = "\"mac\":\"";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::size_t valueStart = start + needle.size();
    const std::size_t valueEnd = json.find('"', valueStart);
    if (valueEnd == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }
    const std::string_view mac = json.substr(valueStart, valueEnd - valueStart);
    if (!isValidBt1035Mac(mac)) {
        return std::unexpected(ParseError::InvalidJson);
    }
    std::string normalized;
    normalized.reserve(12U);
    for (const char ch : mac) {
        normalized.push_back(
            static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

BluetoothConnectRequest parseBluetoothConnectRequest(std::string_view json)
{
    BluetoothConnectRequest request{
        .mac = {},
        .name = {},
        .save = false,
    };
    if (auto mac = parseBluetoothConnectJson(json); mac) {
        request.mac = std::move(*mac);
    }
    const std::string nameNeedle = "\"name\":\"";
    const std::size_t nameStart = json.find(nameNeedle);
    if (nameStart != std::string_view::npos) {
        const std::size_t valueStart = nameStart + nameNeedle.size();
        const std::size_t valueEnd = json.find('"', valueStart);
        if (valueEnd != std::string_view::npos) {
            request.name.assign(json.substr(valueStart, valueEnd - valueStart));
        }
    }
    request.save = json.find("\"save\":true") != std::string_view::npos
                   || json.find("\"save\": true") != std::string_view::npos;
    return request;
}

std::string serializeBluetoothSpeakerJson(const BtSpeakerTarget* target)
{
    if (target == nullptr) {
        return "{\"configured\":false}";
    }
    std::ostringstream out;
    out << "{\"configured\":true,\"mac\":";
    appendJsonString(out, target->mac);
    out << ",\"name\":";
    appendJsonString(out, target->name);
    out << '}';
    return out.str();
}

std::expected<BtSpeakerTarget, ParseError>
parseBluetoothSpeakerJson(std::string_view json)
{
    const auto mac = parseBluetoothConnectJson(json);
    if (!mac) {
        return std::unexpected(mac.error());
    }
    BtSpeakerTarget target{.mac = *mac, .name = {}};
    const std::string nameNeedle = "\"name\":\"";
    const std::size_t nameStart = json.find(nameNeedle);
    if (nameStart != std::string_view::npos) {
        const std::size_t valueStart = nameStart + nameNeedle.size();
        const std::size_t valueEnd = json.find('"', valueStart);
        if (valueEnd != std::string_view::npos) {
            target.name.assign(json.substr(valueStart, valueEnd - valueStart));
        }
    }
    return target;
}

} // namespace core
