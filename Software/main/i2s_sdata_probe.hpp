/**
 * @file    i2s_sdata_probe.hpp
 * @brief   One-shot I2S slave RX capture to check for real audio on SDATA.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

namespace i2s_sdata_probe {

/**
 * Capture I2S slave RX on board_pins BCLK/LRCLK/I2sDataOut for windowMs and
 * log whether real, non-silent PCM content is present. Requires GPIO16
 * (I2sDataOut) to be physically rewired from its old ESP32->ADAU MP1 net to
 * the line under test (e.g. ADAU MP6 -> BT1035 PCM input).
 *
 * @param windowMs  Capture duration in milliseconds.
 */
void captureAndLog(int windowMs);

} // namespace i2s_sdata_probe
