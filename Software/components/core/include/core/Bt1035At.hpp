/**
 * @file    Bt1035At.hpp
 * @brief   Typed AT command builder and response parser for FSC-BT1035.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/ParseError.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    Bt1035AtCommand — supported AT commands (enumerated subset).
 *
 * @dname    Bt1035AtCommand
 * @return   n/a (type)
 * @pubstate Each variant maps to one line via buildBt1035AtLine().
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Bt1035AtCommand {
    Ping,             ///< AT — link check.
    AuxLineIn,        ///< AT+AUXCFG=1 — wired Line-In from ADAU1701 (mandatory).
    PairDiscoverable, ///< AT+PAIR=1 — enter BR/EDR/BLE discoverable mode.
    PairHidden,       ///< AT+PAIR=0 — leave discoverable mode.
    A2dpStat,         ///< AT+A2DPSTAT — read A2DP link state.
    A2dpDisconnect,   ///< AT+A2DPDISC — release current A2DP connection.
};

/**
 * @brief    Bt1035A2dpState — A2DP link state from +A2DPSTAT=Param (BT1035 manual).
 *
 * @dname    Bt1035A2dpState
 * @return   n/a (type)
 * @pubstate Numeric values match Feasycom firmware event codes.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Bt1035A2dpState : std::uint8_t {
    Unsupported = 0,
    Standby = 1,
    Connecting = 2,
    Connected = 3,
    Streaming = 4,
    Paused = 5,
};

/**
 * @brief    Bt1035AtResponseKind — parsed module reply class.
 *
 * @dname    Bt1035AtResponseKind
 * @return   n/a (type)
 * @pubstate Unknown payloads map to Unexpected, never ignored.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Bt1035AtResponseKind {
    Ok,
    Error,
    Unexpected,
};

/** Number of commands in bootInitSequence(). */
inline constexpr std::size_t kBt1035BootInitCommandCount = 2U;

/**
 * @brief    buildBt1035AtLine — serialise a command with CRLF terminator.
 *
 * @dname    buildBt1035AtLine
 * @param    command  Typed AT command.
 * @return   Full line including \r\n suffix.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string buildBt1035AtLine(Bt1035AtCommand command);

/**
 * @brief    bootInitSequence — mandatory bring-up commands in order.
 *
 * @dname    bootInitSequence
 * @return   Ping then AuxLineIn (AT+AUXCFG=1).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::array<Bt1035AtCommand, kBt1035BootInitCommandCount>
bootInitSequence() noexcept;

/**
 * @brief    parseBt1035AtResponse — classify a single response line.
 *
 * @dname    parseBt1035AtResponse
 * @param    line  Untrusted UART payload (may include whitespace).
 * @return   Ok, Error, or Unexpected.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] Bt1035AtResponseKind parseBt1035AtResponse(
    std::string_view line) noexcept;

/**
 * @brief    parseBt1035A2dpStatResponse — extract A2DP state from UART payload.
 *
 * @dname    parseBt1035A2dpStatResponse
 * @param    response  Full module reply (may include +A2DPSTAT and OK lines).
 * @return   Bt1035A2dpState on success, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<Bt1035A2dpState, ParseError>
parseBt1035A2dpStatResponse(std::string_view response);

/**
 * @brief    a2dpStateToken — serialise A2DP state for JSON APIs.
 *
 * @dname    a2dpStateToken
 * @param    state  Parsed A2DP link state.
 * @return   Short stable string (e.g. "streaming").
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] const char* a2dpStateToken(Bt1035A2dpState state) noexcept;

} // namespace core
