/**
 * @file    MixerState.hpp
 * @brief   ADAU1701 input and stereo-mixer gain snapshot.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/GainDb.hpp"

namespace core {

/**
 * @brief    MixerState — per-source and St Mixer1 levels for the ADAU1701.
 *
 * @dname    MixerState
 * @return   n/a (type)
 * @pubstate Immutable snapshot applied by AudioService via IDsp.
 *
 * Maps to SigmaStudio modules Si4674, ESP32, and St Mixer1.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct MixerState {
    GainDb si4684Left;   ///< Si4674 volume control, left channel.
    GainDb si4684Right;  ///< Si4674 volume control, right channel.
    GainDb esp32Left;    ///< ESP32 volume control, left channel.
    GainDb esp32Right;   ///< ESP32 volume control, right channel.
    GainDb mixLeft;      ///< St Mixer1 ST0 (Si4684) input level.
    GainDb mixRight;     ///< St Mixer1 ST1 (ESP32) input level.

    /**
     * @brief    factoryDefault — unity gains on all paths.
     *
     * @dname    factoryDefault
     * @return   MixerState with 0 dB on every control.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static MixerState factoryDefault() noexcept;

    /**
     * @brief    radioFirst — Si4684 path open, ESP32 path muted.
     *
     * @dname    radioFirst
     * @return   MixerState for FM/DAB tuner audio to the Bose I2S chain.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] static MixerState radioFirst() noexcept;
};

} // namespace core
