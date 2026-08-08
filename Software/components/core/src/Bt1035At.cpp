/**
 * @file    Bt1035At.cpp
 * @brief   Bt1035At implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/Bt1035At.hpp"

#include <cctype>
#include <cstdlib>
#include <vector>

namespace core {

namespace {

[[nodiscard]] std::string_view trimAscii(std::string_view text) noexcept
{
    while (!text.empty()
           && (text.front() == ' ' || text.front() == '\r'
               || text.front() == '\n' || text.front() == '\t')) {
        text.remove_prefix(1U);
    }
    while (!text.empty()
           && (text.back() == ' ' || text.back() == '\r' || text.back() == '\n'
               || text.back() == '\t')) {
        text.remove_suffix(1U);
    }
    return text;
}

[[nodiscard]] bool lineEqualsOk(std::string_view line) noexcept
{
    return trimAscii(line) == "OK";
}

[[nodiscard]] bool lineIsError(std::string_view line) noexcept
{
    const std::string_view trimmed = trimAscii(line);
    return trimmed == "ERROR" || trimmed.starts_with("ERROR");
}

[[nodiscard]] bool responseContainsOk(std::string_view response) noexcept
{
    std::size_t start = 0;
    while (start < response.size()) {
        const std::size_t end = response.find_first_of("\r\n", start);
        const std::string_view line =
            end == std::string_view::npos
                ? response.substr(start)
                : response.substr(start, end - start);
        if (lineEqualsOk(line)) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
        while (start < response.size()
               && (response[start] == '\r' || response[start] == '\n')) {
            ++start;
        }
    }
    return lineEqualsOk(response);
}

[[nodiscard]] bool responseContainsError(std::string_view response) noexcept
{
    std::size_t start = 0;
    while (start < response.size()) {
        const std::size_t end = response.find_first_of("\r\n", start);
        const std::string_view line =
            end == std::string_view::npos
                ? response.substr(start)
                : response.substr(start, end - start);
        if (lineIsError(line)) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1U;
        while (start < response.size()
               && (response[start] == '\r' || response[start] == '\n')) {
            ++start;
        }
    }
    return lineIsError(response);
}

} // namespace

std::string buildBt1035AtLine(Bt1035AtCommand command)
{
    switch (command) {
    case Bt1035AtCommand::Reset:
        return "AT+RESET\r\n";
    case Bt1035AtCommand::Ping:
        return "AT\r\n";
    case Bt1035AtCommand::I2sMode:
        return "AT+AUXCFG=3\r\n";
    case Bt1035AtCommand::I2sSlave48k24:
        return "AT+I2SCFG=35\r\n";
    case Bt1035AtCommand::PairDiscoverable:
        return "AT+PAIR=1\r\n";
    case Bt1035AtCommand::PairHidden:
        return "AT+PAIR=0\r\n";
    case Bt1035AtCommand::A2dpStat:
        return "AT+A2DPSTAT\r\n";
    case Bt1035AtCommand::A2dpEncoder:
        return "AT+A2DPENC\r\n";
    case Bt1035AtCommand::A2dpDisconnect:
        return "AT+A2DPDISC\r\n";
    case Bt1035AtCommand::QueryName:
        return "AT+NAME\r\n";
    case Bt1035AtCommand::QueryAutoConn:
        return "AT+AUTOCONN\r\n";
    case Bt1035AtCommand::QueryPairedList:
        return "AT+PLIST\r\n";
    }
    return "AT\r\n";
}

std::string buildBt1035SetAutoConnLine(std::uint8_t times)
{
    if (times > 15U) {
        times = 15U;
    }
    return "AT+AUTOCONN=" + std::to_string(times) + "\r\n";
}

std::string buildBt1035SetNameLine(std::string_view name, bool enableMacSuffix)
{
    if (name.empty() || name.size() > 31U) {
        return {};
    }
    return std::string("AT+NAME=") + std::string(name) + ","
           + (enableMacSuffix ? "1" : "0") + "\r\n";
}

std::array<Bt1035AtCommand, kBt1035BootInitCommandCount> bootInitSequence() noexcept
{
    return std::array<Bt1035AtCommand, kBt1035BootInitCommandCount>{
        Bt1035AtCommand::Ping,
        Bt1035AtCommand::I2sMode,
        Bt1035AtCommand::I2sSlave48k24,
    };
}

Bt1035AtResponseKind parseBt1035AtResponse(std::string_view line) noexcept
{
    if (responseContainsOk(line)) {
        return Bt1035AtResponseKind::Ok;
    }
    if (responseContainsError(line)) {
        return Bt1035AtResponseKind::Error;
    }
    const std::string_view trimmed = trimAscii(line);
    if (trimmed == "OK") {
        return Bt1035AtResponseKind::Ok;
    }
    if (trimmed == "ERROR" || trimmed.starts_with("ERROR")) {
        return Bt1035AtResponseKind::Error;
    }
    return Bt1035AtResponseKind::Unexpected;
}

std::expected<Bt1035A2dpState, ParseError>
parseBt1035A2dpStatResponse(std::string_view response)
{
    constexpr std::string_view kPrefix = "+A2DPSTAT=";
    const std::size_t pos = response.find(kPrefix);
    if (pos == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }

    const std::size_t valueStart = pos + kPrefix.size();
    char* end = nullptr;
    const unsigned long raw =
        std::strtoul(response.data() + valueStart, &end, 10);
    if (end == response.data() + valueStart || raw > 5U) {
        return std::unexpected(ParseError::MissingField);
    }

    return static_cast<Bt1035A2dpState>(raw);
}

const char* a2dpStateToken(Bt1035A2dpState state) noexcept
{
    switch (state) {
    case Bt1035A2dpState::Unsupported:
        return "unsupported";
    case Bt1035A2dpState::Standby:
        return "standby";
    case Bt1035A2dpState::Connecting:
        return "connecting";
    case Bt1035A2dpState::Connected:
        return "connected";
    case Bt1035A2dpState::Streaming:
        return "streaming";
    case Bt1035A2dpState::Paused:
        return "paused";
    }
    return "unknown";
}

std::expected<Bt1035A2dpCodec, ParseError>
parseBt1035A2dpEncoderResponse(std::string_view response)
{
    constexpr std::string_view kPrefix = "+A2DPENC=";
    const std::size_t pos = response.find(kPrefix);
    if (pos == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }

    const std::size_t valueStart = pos + kPrefix.size();
    char* end = nullptr;
    const unsigned long raw =
        std::strtoul(response.data() + valueStart, &end, 10);
    if (end == response.data() + valueStart || raw < 1U || raw > 5U) {
        return std::unexpected(ParseError::MissingField);
    }

    return static_cast<Bt1035A2dpCodec>(raw);
}

const char* a2dpCodecToken(Bt1035A2dpCodec codec) noexcept
{
    switch (codec) {
    case Bt1035A2dpCodec::Sbc:
        return "sbc";
    case Bt1035A2dpCodec::Aptx:
        return "aptx";
    case Bt1035A2dpCodec::AptxHd:
        return "aptx-hd";
    case Bt1035A2dpCodec::AptxLl:
        return "aptx-ll";
    case Bt1035A2dpCodec::AptxAdaptive:
        return "aptx-adaptive";
    }
    return "unknown";
}

std::expected<std::string, ParseError>
parseBt1035NameResponse(std::string_view response)
{
    constexpr std::string_view kPrefix = "+NAME=";
    const std::size_t pos = response.find(kPrefix);
    if (pos == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::size_t start = pos + kPrefix.size();
    const std::size_t end = response.find_first_of("\r\n", start);
    const std::string_view value =
        end == std::string_view::npos ? response.substr(start)
                                      : response.substr(start, end - start);
    if (value.empty()) {
        return std::unexpected(ParseError::MissingField);
    }
    return std::string(value);
}

std::expected<std::uint8_t, ParseError>
parseBt1035AutoConnResponse(std::string_view response)
{
    constexpr std::string_view kPrefix = "+AUTOCONN=";
    const std::size_t pos = response.find(kPrefix);
    if (pos == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::size_t start = pos + kPrefix.size();
    char* end = nullptr;
    const unsigned long raw =
        std::strtoul(response.data() + start, &end, 10);
    if (end == response.data() + start || raw > 15U) {
        return std::unexpected(ParseError::MissingField);
    }
    return static_cast<std::uint8_t>(raw);
}

std::expected<std::vector<Bt1035PairedDevice>, ParseError>
parseBt1035PairedListResponse(std::string_view response)
{
    std::vector<Bt1035PairedDevice> devices;
    constexpr std::string_view kPrefix = "+PLIST=";
    std::size_t pos = 0;
    while ((pos = response.find(kPrefix, pos)) != std::string_view::npos) {
        const std::size_t start = pos + kPrefix.size();
        const std::size_t end = response.find_first_of("\r\n", start);
        const std::string_view line =
            end == std::string_view::npos ? response.substr(start)
                                          : response.substr(start, end - start);
        pos = end == std::string_view::npos ? response.size() : end + 1U;
        if (line.empty() || line.front() == 'E') {
            continue;
        }
        const std::size_t comma1 = line.find(',');
        if (comma1 == std::string_view::npos) {
            continue;
        }
        const std::size_t comma2 = line.find(',', comma1 + 1U);
        char* endIdx = nullptr;
        const unsigned long index =
            std::strtoul(line.data(), &endIdx, 10);
        if (endIdx == line.data() || index == 0U || index > 8U) {
            continue;
        }
        Bt1035PairedDevice entry = {};
        entry.index = static_cast<std::uint8_t>(index);
        entry.mac = std::string(line.substr(comma1 + 1U,
                                            (comma2 == std::string_view::npos
                                                 ? line.size()
                                                 : comma2)
                                                - comma1
                                                - 1U));
        if (comma2 != std::string_view::npos) {
            entry.name = std::string(line.substr(comma2 + 1U));
        }
        devices.push_back(std::move(entry));
    }
    return devices;
}

std::string buildBt1035StartScanLine(std::uint8_t scanType, std::uint8_t scanSeconds)
{
    const std::uint8_t type = (scanType == 2U) ? 2U : 1U;
    if (scanSeconds == 0U) {
        return "AT+SCAN=" + std::to_string(type) + "\r\n";
    }
    return "AT+SCAN=" + std::to_string(type) + ","
           + std::to_string(scanSeconds) + "\r\n";
}

std::string buildBt1035StopScanLine()
{
    return "AT+SCAN=0\r\n";
}

std::string buildBt1035EnablePrintLine()
{
    return "AT+PRINT=1\r\n";
}

std::string buildBt1035DisconnectAllLine()
{
    return "AT+DSCA\r\n";
}

std::string buildBt1035DisableAutoLinkLine()
{
    return "AT+LINKCFG=0,0\r\n";
}

std::string buildBt1035ClearPairedListLine()
{
    return "AT+PLIST=0\r\n";
}

bool isValidBt1035Mac(std::string_view mac) noexcept
{
    if (mac.size() != 12U) {
        return false;
    }
    for (const char ch : mac) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string normalizeBt1035Mac(std::string_view mac)
{
    std::string normalized;
    normalized.reserve(12U);
    for (const char ch : mac) {
        if (ch == ':' || ch == '-') {
            continue;
        }
        normalized.push_back(
            static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

std::string buildBt1035A2dpConnectLine(std::string_view mac)
{
    if (!isValidBt1035Mac(mac)) {
        return {};
    }
    return std::string("AT+A2DPCONN=") + std::string(mac) + "\r\n";
}

std::string buildBt1035A2dpAudioLine(bool establish)
{
    return establish ? "AT+A2DPAUDIO=1\r\n" : "AT+A2DPAUDIO=0\r\n";
}

std::expected<std::vector<Bt1035ScannedDevice>, ParseError>
parseBt1035ScanResponse(std::string_view response)
{
    std::vector<Bt1035ScannedDevice> devices;
    std::size_t pos = 0;
    while ((pos = response.find("+SCAN", pos)) != std::string_view::npos) {
        std::size_t start = pos + 5U;
        if (start < response.size() && response[start] == '=') {
            ++start;
        }
        while (start < response.size()
               && (response[start] == ' ' || response[start] == '\t')) {
            ++start;
        }
        const std::size_t end = response.find_first_of("\r\n", start);
        const std::string_view line =
            end == std::string_view::npos ? response.substr(start)
                                          : response.substr(start, end - start);
        pos = end == std::string_view::npos ? response.size() : end + 1U;
        if (line.empty() || line == "E" || line.front() == 'E') {
            continue;
        }

        const std::string lineStr(line);

        const std::size_t comma1 = lineStr.find(',');
        const std::size_t comma2 =
            comma1 == std::string_view::npos ? std::string_view::npos
                                             : lineStr.find(',', comma1 + 1U);
        const std::size_t comma3 =
            comma2 == std::string_view::npos ? std::string_view::npos
                                             : lineStr.find(',', comma2 + 1U);
        const std::size_t comma4 =
            comma3 == std::string_view::npos ? std::string_view::npos
                                             : lineStr.find(',', comma3 + 1U);
        if (comma4 == std::string_view::npos) {
            continue;
        }

        char* endIdx = nullptr;
        const unsigned long index =
            std::strtoul(lineStr.c_str(), &endIdx, 10);
        if (endIdx == lineStr.c_str() || index == 0U) {
            continue;
        }

        const unsigned long addrType =
            std::strtoul(lineStr.c_str() + comma1 + 1U, &endIdx, 10);
        const std::string mac =
            normalizeBt1035Mac(lineStr.substr(comma2 + 1U, comma3 - comma2 - 1U));
        if (!isValidBt1035Mac(mac)) {
            continue;
        }

        const long rssi =
            std::strtol(lineStr.c_str() + comma3 + 1U, &endIdx, 10);
        const unsigned long nameLen =
            std::strtoul(lineStr.c_str() + comma4 + 1U, &endIdx, 10);
        const std::size_t afterNameLen =
            static_cast<std::size_t>(endIdx - lineStr.c_str());

        std::string name;
        std::string deviceClass;
        if (nameLen > 0U) {
            if (afterNameLen >= lineStr.size() || lineStr[afterNameLen] != ',') {
                continue;
            }
            const std::size_t nameStart = afterNameLen + 1U;
            if (nameStart + nameLen > lineStr.size()) {
                continue;
            }
            name.assign(lineStr.substr(nameStart, nameLen));
            const std::size_t afterName = nameStart + nameLen;
            if (afterName < lineStr.size() && lineStr[afterName] == ',') {
                deviceClass.assign(lineStr.substr(afterName + 1U));
            }
        } else if (afterNameLen < lineStr.size()
                   && lineStr[afterNameLen] == ',') {
            const std::size_t classStart = afterNameLen + 1U;
            if (classStart < lineStr.size()) {
                if (lineStr[classStart] == ',') {
                    deviceClass.assign(lineStr.substr(classStart + 1U));
                } else {
                    deviceClass.assign(lineStr.substr(classStart));
                }
            }
        }

        Bt1035ScannedDevice entry = {};
        entry.index = static_cast<std::uint8_t>(index);
        entry.addressType = static_cast<std::uint8_t>(addrType);
        entry.mac = mac;
        entry.rssiDbm = static_cast<std::int16_t>(rssi);
        entry.name = std::move(name);
        entry.deviceClass = std::move(deviceClass);
        devices.push_back(std::move(entry));
    }
    return devices;
}

} // namespace core
