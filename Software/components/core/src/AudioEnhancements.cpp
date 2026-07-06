/**
 * @file    AudioEnhancements.cpp
 * @brief   AudioEnhancements factory defaults.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/AudioEnhancements.hpp"

namespace core {

AudioEnhancements AudioEnhancements::factoryDefault() noexcept
{
    return AudioEnhancements{
        .stereo = EnhanceLevel::zero(),
        .bass = EnhanceLevel::zero(),
    };
}

} // namespace core
