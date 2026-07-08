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

#include "core/Bt1035PairedDevice.hpp"
#include "core/ParseError.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

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
    I2sMode,          ///< AT+AUXCFG=3 — I2S input from ADAU1701 (mandatory).
    I2sSlave48k32,    ///< AT+I2SCFG=67 — I2S slave 48 kHz 32-bit (§5.1.4).
    PairDiscoverable, ///< AT+PAIR=1 — enter BR/EDR/BLE discoverable mode.
    PairHidden,       ///< AT+PAIR=0 — leave discoverable mode.
    A2dpStat,         ///< AT+A2DPSTAT — read A2DP link state.
    A2dpDisconnect,   ///< AT+A2DPDISC — release current A2DP connection.
    QueryName,        ///< AT+NAME — read BR/EDR local name (+NAME=).
    QueryAutoConn,    ///< AT+AUTOCONN — read power-on auto-reconnect count.
    QueryPairedList,  ///< AT+PLIST — enumerate paired devices (+PLIST=).
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
inline constexpr std::size_t kBt1035BootInitCommandCount = 3U;

/** Feasycom programming guide §5.1.4: I2S slave, 48 kHz, 32-bit. */
inline constexpr std::uint8_t kBt1035I2sSlave48k32Param = 67U;

/**
 * @brief    buildBt1035AtLine — serialise a command with CRLF terminator.
 *
 * @dname    buildBt1035AtLine
 * @param    command  Typed AT command.
 * @return   Full line including CRLF suffix.
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
 * @return   Ping, I2sMode (AUXCFG=3), I2sSlave48k32 (I2SCFG=67).
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

/**
 * @brief    buildBt1035SetAutoConnLine — AT+AUTOCONN with reconnect count.
 *
 * @dname    buildBt1035SetAutoConnLine
 * @param    times  0 off, 1–15 reconnect attempts (Feasycom default 3).
 * @return   Full AT line including CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::string buildBt1035SetAutoConnLine(std::uint8_t times);

/**
 * @brief    buildBt1035SetNameLine — AT+NAME with optional MAC suffix flag.
 *
 * @dname    buildBt1035SetNameLine
 * @param    name  BR/EDR local name (1--31 ASCII bytes per Feasycom §5.1.16).
 * @param    enableMacSuffix  false sends Param2=0 (disable module suffix).
 * @return   Full AT line including CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-08
 */
[[nodiscard]] std::string buildBt1035SetNameLine(std::string_view name,
                                                 bool enableMacSuffix = false);

/**
 * @brief    parseBt1035NameResponse — extract +NAME= value.
 *
 * @dname    parseBt1035NameResponse
 * @param    response  Full module reply ending in OK.
 * @return   Device name on success, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<std::string, ParseError>
parseBt1035NameResponse(std::string_view response);

/**
 * @brief    parseBt1035AutoConnResponse — extract +AUTOCONN= value.
 *
 * @dname    parseBt1035AutoConnResponse
 * @param    response  Full module reply ending in OK.
 * @return   Reconnect count 0–15, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<std::uint8_t, ParseError>
parseBt1035AutoConnResponse(std::string_view response);

/**
 * @brief    parseBt1035PairedListResponse — parse +PLIST= lines.
 *
 * @dname    parseBt1035PairedListResponse
 * @param    response  Full module reply ending in OK.
 * @return   Paired devices in index order; empty when none stored.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<std::vector<Bt1035PairedDevice>, ParseError>
parseBt1035PairedListResponse(std::string_view response);

} // namespace core
