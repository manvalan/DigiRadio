/**
 * @file    WebRadioBudget.hpp
 * @brief   Choose the web radio ring size from the free heap.
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
 * @date    2026-10-27
 */
#pragma once

#include <array>
#include <cstddef>
#include <optional>

namespace core {

/** Ring sizes tried, largest first (24 KB ≈ 1.5 s at 128 kbit/s). */
inline constexpr std::array<std::size_t, 3> kWebRadioRingSizes{
    24U * 1024U, 16U * 1024U, 12U * 1024U};

/** Heap that must stay free after the ring for an http stream: MP3
 *  decoder (~23 KB in eight blocks), web_radio_net stack (10 KB), HTTP
 *  client and socket (~3 KB), Wi-Fi RX buffers in flight (~10 KB with a
 *  5760 B TCP window) and ~6 KB for HTTP requests and other tasks. In
 *  0.12.3 a start with ~49 KB left after the ring ended at 1.6 KB; 0.12.4
 *  measured ~49 KB used on top of the ring with 10-12 KB left, so 64 KB
 *  keeps about 16 KB free while streaming (0.12.5). */
inline constexpr std::size_t kWebRadioHttpHeadroom = 64U * 1024U;

/** Extra headroom for https: mbedTLS records and handshake (80 KB in
 *  total, so https only fits on a module with PSRAM in practice). */
inline constexpr std::size_t kWebRadioTlsHeadroom = 16U * 1024U;

/**
 * @brief    chooseWebRadioRing — the ring size the heap can afford.
 *
 * @dname    chooseWebRadioRing
 * @param    freeBytes     Total free internal heap.
 * @param    largestBlock  Largest free block (the ring is one block).
 * @param    https         Stream URL uses TLS.
 * @return   The largest size in kWebRadioRingSizes that fits in one block
 *           and leaves the headroom free; nullopt when even the smallest
 *           would not (start refused with OutOfMemory instead of letting
 *           Wi-Fi run out, as 0.12.3 did with 1.6 KB left).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::optional<std::size_t> chooseWebRadioRing(
    std::size_t freeBytes, std::size_t largestBlock, bool https) noexcept;

} // namespace core
