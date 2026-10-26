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
 *  decoder (~25 KB), web_radio_net stack (10 KB), HTTP client (2 KB) and
 *  Wi-Fi/lwIP receive buffers for the stream. */
inline constexpr std::size_t kWebRadioHttpHeadroom = 40U * 1024U;

/** Extra headroom for https: mbedTLS records and handshake. */
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
