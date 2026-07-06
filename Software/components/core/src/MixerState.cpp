/**
 * @file    MixerState.cpp
 * @brief   MixerState factory defaults.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/MixerState.hpp"

namespace core {

MixerState MixerState::factoryDefault() noexcept
{
    const GainDb unity = GainDb::zero();
    return MixerState{
        .si4684Left = unity,
        .si4684Right = unity,
        .esp32Left = unity,
        .esp32Right = unity,
        .mixLeft = unity,
        .mixRight = unity,
    };
}

} // namespace core
