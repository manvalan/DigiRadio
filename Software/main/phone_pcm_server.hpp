/**
 * @file    phone_pcm_server.hpp
 * @brief   Raw PCM audio from the phone on a dedicated TCP port.
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

#include <cstdint>

namespace phone_pcm_server {

/** TCP port the app connects to (advertised as _digiradio-pcm._tcp). */
inline constexpr std::uint16_t kPort = 5005U;

/**
 * @brief    start — spawn the listener task and advertise it over mDNS.
 *
 * Protocol: the client connects and sends interleaved little-endian
 * 16-bit stereo PCM at 48 kHz, nothing else, until it closes the socket.
 * One client at a time; a second one waits in the listen backlog.
 *
 * Replaces PUT /api/stream/phone, which never worked as the app used it:
 * esp_http_server does not decode chunked request bodies, and the upload
 * held the server's only worker so every other API call stalled.
 *
 * @dname    start
 * @return   false if the task could not be created.
 * @pubstate creates the "phone_pcm" task.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] bool start();

} // namespace phone_pcm_server
