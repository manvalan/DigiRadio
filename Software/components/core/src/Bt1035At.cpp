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

#include <cstdlib>

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
    case Bt1035AtCommand::Ping:
        return "AT\r\n";
    case Bt1035AtCommand::AuxLineIn:
        return "AT+AUXCFG=1\r\n";
    case Bt1035AtCommand::PairDiscoverable:
        return "AT+PAIR=1\r\n";
    case Bt1035AtCommand::PairHidden:
        return "AT+PAIR=0\r\n";
    case Bt1035AtCommand::A2dpStat:
        return "AT+A2DPSTAT\r\n";
    case Bt1035AtCommand::A2dpDisconnect:
        return "AT+A2DPDISC\r\n";
    }
    return "AT\r\n";
}

std::array<Bt1035AtCommand, kBt1035BootInitCommandCount> bootInitSequence() noexcept
{
    return std::array<Bt1035AtCommand, kBt1035BootInitCommandCount>{
        Bt1035AtCommand::Ping,
        Bt1035AtCommand::AuxLineIn,
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

} // namespace core
