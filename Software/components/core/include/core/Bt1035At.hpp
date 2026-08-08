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
#include "core/Bt1035ScannedDevice.hpp"
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
    Reset,            ///< AT+RESET — software reset (best-effort, not gated on OK).
    Ping,             ///< AT — link check.
    I2sMode,          ///< AT+AUXCFG=3 — I2S input from ADAU1701 (mandatory).
    I2sSlave48k24,    ///< AT+I2SCFG=35 — I2S slave 48 kHz 24-bit (§5.1.4).
    PairDiscoverable, ///< AT+PAIR=1 — enter BR/EDR/BLE discoverable mode.
    PairHidden,       ///< AT+PAIR=0 — leave discoverable mode.
    A2dpStat,         ///< AT+A2DPSTAT — read A2DP link state.
    A2dpEncoder,      ///< AT+A2DPENC — read negotiated A2DP codec (§5.3.5).
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
 * @brief    Bt1035A2dpCodec — negotiated codec from +A2DPENC=Param (§5.3.5).
 *
 * @dname    Bt1035A2dpCodec
 * @return   n/a (type)
 * @pubstate BT1035 chip variant codes (BT806 differs; board uses BT1035).
 *
 * @author   Michele Bigi
 * @date     2026-08-07
 */
enum class Bt1035A2dpCodec : std::uint8_t {
    Sbc = 1,
    Aptx = 2,
    AptxHd = 3,
    AptxLl = 4,
    AptxAdaptive = 5,
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

/**
 * Feasycom programming guide §5.1.4: I2S slave, 48 kHz, 24-bit — matches the
 * ADAU1701 serial output word length (SigmaStudio Hardware Configuration).
 * Bit field: BIT[0]=enable(1), BIT[1]=slave(1), BIT[2]=FS 48kHz(0),
 * BIT[3]=left justified(0), BIT[4]=data 1-bit delay(0), BIT[5:6]=24-bit(01)
 * -> 1 + 2 + 32 = 35.
 */
inline constexpr std::uint8_t kBt1035I2sSlave48k24Param = 35U;

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
 * @return   Ping, I2sMode (AUXCFG=3), I2sSlave48k24 (I2SCFG=35).
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
 * @brief    parseBt1035A2dpEncoderResponse — extract negotiated codec.
 *
 * @dname    parseBt1035A2dpEncoderResponse
 * @param    response  Full module reply (may include +A2DPENC and OK lines).
 * @return   Bt1035A2dpCodec on success, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-07
 */
[[nodiscard]] std::expected<Bt1035A2dpCodec, ParseError>
parseBt1035A2dpEncoderResponse(std::string_view response);

/**
 * @brief    a2dpCodecToken — serialise negotiated codec for logs/JSON.
 *
 * @dname    a2dpCodecToken
 * @param    codec  Parsed negotiated codec.
 * @return   Short stable string (e.g. "sbc").
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-07
 */
[[nodiscard]] const char* a2dpCodecToken(Bt1035A2dpCodec codec) noexcept;

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

/**
 * @brief    buildBt1035StartScanLine — AT+SCAN for nearby BR/EDR devices.
 *
 * @dname    buildBt1035StartScanLine
 * @param    scanType     AT+SCAN Param1; clamped to 1 or 2 (2 only when
 *                        passed exactly, otherwise 1).
 * @param    scanSeconds  BR/EDR inquiry time 1–255 (Feasycom Param2).
 * @return   Full AT line including CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035StartScanLine(std::uint8_t scanType,
                                                 std::uint8_t scanSeconds = 0U);

/**
 * @brief    buildBt1035StopScanLine — stop an active device scan.
 *
 * @dname    buildBt1035StopScanLine
 * @return   AT+SCAN=0 line with CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035StopScanLine();

/**
 * @brief    buildBt1035EnablePrintLine — ensure +SCAN events are reported.
 *
 * @dname    buildBt1035EnablePrintLine
 * @return   AT+PRINT=1 line with CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035EnablePrintLine();

/**
 * @brief    buildBt1035DisconnectAllLine — release all BT connections.
 *
 * @dname    buildBt1035DisconnectAllLine
 * @return   AT+DSCA line with CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035DisconnectAllLine();

/**
 * @brief    buildBt1035DisableAutoLinkLine — disable auto-link on power-up.
 *
 * @dname    buildBt1035DisableAutoLinkLine
 * @return   AT+LINKCFG=0,0 line with CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035DisableAutoLinkLine();

/**
 * @brief    buildBt1035ClearPairedListLine — erase all paired records.
 *
 * @dname    buildBt1035ClearPairedListLine
 * @return   AT+PLIST=0 line with CRLF.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035ClearPairedListLine();

/**
 * @brief    buildBt1035A2dpConnectLine — connect A2DP to a remote MAC.
 *
 * @dname    buildBt1035A2dpConnectLine
 * @param    mac  12-char ASCII MAC address (no colons).
 * @return   AT+A2DPCONN line, or empty when mac invalid.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035A2dpConnectLine(std::string_view mac);

/**
 * @brief    buildBt1035A2dpAudioLine — start/stop A2DP audio (§5.3.6).
 *
 * @dname    buildBt1035A2dpAudioLine
 * @param    establish  true → AT+A2DPAUDIO=1, false → release audio.
 * @return   Full AT line including CRLF.
 * @pubstate none
 *
 * Feasycom BT1035: link state Connected (3) is not enough for I2S→sink
 * playback; audio starts after A2DPAUDIO=1 and +A2DPSTAT=4 (Streaming).
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string buildBt1035A2dpAudioLine(bool establish);

/**
 * @brief    isValidBt1035Mac — true for 12 hex digits.
 *
 * @dname    isValidBt1035Mac
 * @param    mac  Candidate MAC from scan or UI.
 * @return   true when exactly 12 hexadecimal characters.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] bool isValidBt1035Mac(std::string_view mac) noexcept;

/**
 * @brief    normalizeBt1035Mac — strip separators and uppercase MAC.
 *
 * @dname    normalizeBt1035Mac
 * @param    mac  MAC with optional colons or dashes.
 * @return   12 uppercase hex digits, or empty when invalid length/chars.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string normalizeBt1035Mac(std::string_view mac);

/**
 * @brief    parseBt1035ScanResponse — parse +SCAN= inquiry results.
 *
 * @dname    parseBt1035ScanResponse
 * @param    response  Full UART payload until +SCAN=E.
 * @return   Discovered devices, or ParseError when payload malformed.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::expected<std::vector<Bt1035ScannedDevice>, ParseError>
parseBt1035ScanResponse(std::string_view response);

} // namespace core
