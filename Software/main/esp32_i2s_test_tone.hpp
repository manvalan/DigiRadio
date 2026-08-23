/**
 * @file    esp32_i2s_test_tone.hpp
 * @brief   Continuous sine tone over the shared ESP32 -> ADAU1701 I2S TX
 *          channel, for isolating ADAU1701 I2S-input problems from the
 *          Si4684 specifically.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

namespace esp32_i2s_test_tone {

/**
 * Generate and write a sine-wave tone over esp32_i2s_sink forever. Intended
 * to run as its own FreeRTOS task (never returns). Unlike the ADAU1701's
 * internal Beep cell (POST /api/audio/beep), this tone travels over the
 * physical SDATA_IN1 wire and the shared BCLK/LRCLK the Si4684 also uses on
 * SDATA_IN0 -- if it sounds clean while FM/DAB is distorted, the problem is
 * specific to the Si4684 side, not the ADAU1701's I2S input path in general.
 *
 * @param freqHz  Tone frequency in Hz.
 */
[[noreturn]] void run(float freqHz);

} // namespace esp32_i2s_test_tone
