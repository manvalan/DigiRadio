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
    Ping,      ///< AT — link check.
    AuxLineIn, ///< AT+AUXCFG=1 — wired Line-In from ADAU1701 (mandatory).
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

} // namespace core
