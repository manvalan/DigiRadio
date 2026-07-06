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

} // namespace

std::string buildBt1035AtLine(Bt1035AtCommand command)
{
    switch (command) {
    case Bt1035AtCommand::Ping:
        return "AT\r\n";
    case Bt1035AtCommand::AuxLineIn:
        return "AT+AUXCFG=1\r\n";
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
    const std::string_view trimmed = trimAscii(line);
    if (trimmed == "OK") {
        return Bt1035AtResponseKind::Ok;
    }
    if (trimmed == "ERROR" || trimmed.starts_with("ERROR")) {
        return Bt1035AtResponseKind::Error;
    }
    return Bt1035AtResponseKind::Unexpected;
}

} // namespace core
