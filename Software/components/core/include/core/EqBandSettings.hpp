/**
 * @file    EqBandSettings.hpp
 * @brief   Per-band EQ settings for EqProfile.
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

#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

namespace core {

/**
 * @brief    EqBandSettings — gain, centre frequency, and Q for one PEQ band.
 *
 * @dname    EqBandSettings
 * @return   n/a (type)
 * @pubstate Plain value type; q in 0.2..10.0 when validated via EqProfile.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct EqBandSettings {
    GainDb gain;           ///< Band gain in dB.
    FrequencyHz center;    ///< Centre frequency in Hz.
    float q;               ///< Quality factor.
};

} // namespace core
