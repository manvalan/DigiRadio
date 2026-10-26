/**
 * @file    web_radio_session.hpp
 * @brief   State shared by the web radio network task and player task.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include "core/WebRadioStatus.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>

namespace web_radio_stream {

/**
 * @brief    Session — one URL being played, split across two tasks.
 *
 * The network task (web_radio_fetch.cpp) connects, strips ICY metadata
 * and pushes MP3 bytes into `ring`; the player task decodes from `ring`
 * at the ADAU's pace. The ring absorbs Wi-Fi jitter, and a reconnect
 * happens in the background while the ring plays out.
 *
 * @dname    Session
 * @pubstate fields written by the network task are guarded by `lock`.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct Session {
    std::string url;                  ///< Set by the player before start.
    StreamBufferHandle_t ring{nullptr}; ///< MP3 bytes, network -> player.
    std::size_t ringBytes{0U};          ///< Capacity of ring.
    std::atomic<bool> stop{false};    ///< Player asks the network task to end.
    SemaphoreHandle_t done{nullptr};  ///< Given by the network task on exit.
    SemaphoreHandle_t lock{nullptr};  ///< Guards the fields below.
    core::WebRadioState netState{core::WebRadioState::Connecting}; ///< Net side.
    core::WebRadioFault fault{core::WebRadioFault::None}; ///< Last failure.
    int httpStatus{0};             ///< Last HTTP status.
    std::string codec;             ///< "mp3" or the refused format.
    std::uint32_t reconnects{0U};  ///< Reconnections this session.
    std::optional<std::string> title; ///< Latest ICY StreamTitle.
};

/**
 * @brief    startFetch — spawn the network task for `session`.
 *
 * @dname    startFetch
 * @param    session  Prepared session (url, ring, semaphores).
 * @return   false if the task could not be created.
 * @pubstate the task gives session.done when it exits.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool startFetch(Session& session);

} // namespace web_radio_stream
