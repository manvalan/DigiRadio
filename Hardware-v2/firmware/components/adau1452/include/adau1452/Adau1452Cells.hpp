/**
 * @file    Adau1452Cells.hpp
 * @brief   Parameter addresses of the DigiRadio v2 DSP program.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Addresses come from the SigmaStudio+ compilation and change with every
 * program, so they are generated: tools/sigma_export.py writes
 * dsp/adau1452_cells.inc. Without that file nothing is assumed: kAvailable
 * is false and every write reports CellsMissing.
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include <array>
#include <cstdint>

namespace adau1452::cells {

#if __has_include("adau1452_cells.inc")
#include "adau1452_cells.inc"
inline constexpr bool kAvailable = true;
#else
inline constexpr bool kAvailable = false;
inline constexpr std::uint16_t kSourceSelect = 0U;
inline constexpr std::uint16_t kMasterLeft = 0U;
inline constexpr std::uint16_t kMasterRight = 0U;
inline constexpr std::uint16_t kBeepEnable = 0U;
inline constexpr std::uint16_t kBassAmount = 0U;
inline constexpr std::uint16_t kStereoAmount = 0U;
/** First of five cells per band, in the order b0 b1 b2 a1 a2. */
inline constexpr std::array<std::uint16_t, 6> kEqBand = {};
/** Radio L/R, ESP32 L/R, output L/R level meters. */
inline constexpr std::array<std::uint16_t, 6> kLevel = {};
#endif

} // namespace adau1452::cells
