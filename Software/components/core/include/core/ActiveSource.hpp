/**
 * @file    ActiveSource.hpp
 * @brief   ADAU1701 MX1 input source selector.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-25
 */
#pragma once

namespace core {

/**
 * @brief    ActiveSource — the one stereo pair MX1 currently passes through.
 *
 * @dname    ActiveSource
 * @return   n/a (type)
 * @pubstate n/a
 *
 * The DigiRadioFinale SigmaStudio revision (2026-08-25) replaced St Mixer1
 * (which combined Si4684 + ESP32 + Beep simultaneously, each at its own
 * gain) with MX1, a DC-controlled exclusive mux: only one of the three
 * stereo pairs reaches Param EQ1 at a time, selected by the DC1 cell.
 * Per-source gain trims (the old Si4674/ESP32 cells) no longer exist —
 * level is controlled solely by Master Volume and the EQ, downstream of
 * the selected source.
 *
 * @author   Michele Bigi
 * @date     2026-08-25
 */
enum class ActiveSource {
    Radio,     ///< Si4684 tuner (FM/DAB), MX1 pair index 0.
    Bluetooth, ///< ESP32 I2S input, MX1 pair index 1.
    Beep,      ///< Internal test tone, MX1 pair index 2.
};

// DC1 is a raw 32-bit integer index (0/1/2), not the 5.23 fixpoint its
// compiled TYPE_DC1/VALUE_DC1 macros would suggest -- confirmed live
// 2026-08-25 (see adau1701::paramSourceIndex() and Adau1701Driver::
// selectSource()).

} // namespace core
