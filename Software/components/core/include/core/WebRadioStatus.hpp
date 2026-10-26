/**
 * @file    WebRadioStatus.hpp
 * @brief   Live state of the internet-radio player for GET /api/streaming.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace core {

/** Where the player is in its connect → buffer → play cycle. */
enum class WebRadioState {
    Idle,       ///< Streaming disabled.
    Connecting, ///< Opening the HTTP(S) connection or resolving a playlist.
    Buffering,  ///< Connected, filling the buffer before (re)starting audio.
    Playing,    ///< Audio flowing to the ADAU1701.
    Error,      ///< Last attempt failed; retrying with back-off.
};

/** Why the last attempt failed (WebRadioState::Error). */
enum class WebRadioFault {
    None,
    ConnectFailed,     ///< DNS, TCP or TLS failure.
    HttpStatus,        ///< Server answered non-2xx (see httpStatus).
    NotAudio,          ///< A web page, not a stream.
    UnsupportedFormat, ///< AAC, HLS or another codec the firmware lacks.
    Decode,            ///< MP3 data would not decode.
    OutOfMemory,       ///< Buffer or TLS allocation failed.
    AudioOutput,       ///< I2S write timed out (no clock from the ADAU).
};

/**
 * @brief    WebRadioStatus — snapshot published by the player task.
 *
 * @dname    WebRadioStatus
 * @return   n/a (type)
 * @pubstate Plain DTO; serialised by serializeWebRadioJson().
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct WebRadioStatus {
    WebRadioState state{WebRadioState::Idle}; ///< Player state.
    WebRadioFault fault{WebRadioFault::None}; ///< Set in Error state.
    int httpStatus{0};               ///< Last HTTP status, 0 if none.
    std::string codec;               ///< "mp3", or what was refused.
    std::uint32_t sampleRateHz{0U};  ///< Stream rate before resampling.
    std::uint8_t channels{0U};       ///< 1 (mono) or 2.
    std::uint16_t bitrateKbps{0U};   ///< From the MP3 frame header.
    std::uint8_t bufferPercent{0U};  ///< Ring buffer fill 0–100.
    std::uint32_t underruns{0U};     ///< Times the buffer ran dry.
    std::uint32_t reconnects{0U};    ///< Reconnections since enable.
    std::optional<std::string> title; ///< ICY StreamTitle (UTF-8).
};

/**
 * @brief    webRadioStateToken — JSON token for a state.
 *
 * @dname    webRadioStateToken
 * @param    state  Player state.
 * @return   "idle", "connecting", "buffering", "playing" or "error".
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string_view webRadioStateToken(WebRadioState state) noexcept;

/**
 * @brief    webRadioFaultToken — JSON token for a fault.
 *
 * @dname    webRadioFaultToken
 * @param    fault  Failure reason.
 * @return   "" for None, else e.g. "http_status", "not_audio".
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string_view webRadioFaultToken(WebRadioFault fault) noexcept;

} // namespace core
