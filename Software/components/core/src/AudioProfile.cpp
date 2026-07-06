/**
 * @file    AudioProfile.cpp
 * @brief   AudioProfile factory defaults.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/AudioProfile.hpp"

namespace core {

AudioProfile AudioProfile::factoryDefault() noexcept
{
    const GainDb unity = GainDb::zero();
    return AudioProfile{
        .mixer = MixerState::factoryDefault(),
        .eq = EqProfile::factoryDefault(),
        .masterLeft = unity,
        .masterRight = unity,
        .enhancements = AudioEnhancements::factoryDefault(),
    };
}

} // namespace core
