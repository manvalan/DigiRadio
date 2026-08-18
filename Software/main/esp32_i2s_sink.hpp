/**
 * @file    esp32_i2s_sink.hpp
 * @brief   Shared ESP32 -> ADAU1701 I2S TX channel (web radio / phone stream).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace esp32_i2s_sink {

/**
 * Open the shared I2S TX channel (ESP32 as I2S slave, ADAU1701 as master)
 * on first call; a no-op on later calls. Not thread-safe to call
 * concurrently with itself — call once during boot.
 * @return true on success.
 */
bool open();

/**
 * Claim exclusive use of the channel for one producer (web radio stream or
 * a phone PCM stream). Only one producer may hold it at a time, since both
 * would otherwise fight over the same physical wire.
 * @return true if acquired, false if another producer already holds it.
 */
bool tryAcquire();

/** Release a previously acquired claim. Safe to call even if not held. */
void release();

/**
 * Write one frame's worth of already-32-bit-slot-formatted stereo samples
 * (left, right pairs). Blocks until the DMA accepts the data.
 * @param samples      Interleaved L,R int32 samples (top-aligned 24-bit).
 * @param sampleCount  Number of int32 values in samples (2x frame count).
 * @return true on success.
 */
bool writeSamples(const std::int32_t* samples, std::size_t sampleCount);

} // namespace esp32_i2s_sink
