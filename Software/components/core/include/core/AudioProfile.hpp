/**
 * @file    AudioProfile.hpp
 * @brief   Combined mixer, EQ, and master volume snapshot.
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

#include "core/EqProfile.hpp"
#include "core/GainDb.hpp"
#include "core/MixerState.hpp"

namespace core {

/**
 * @brief    AudioProfile — full ADAU1701 user configuration snapshot.
 *
 * @dname    AudioProfile
 * @return   n/a (type)
 * @pubstate Plain aggregate persisted by IAudioProfileStore and applied by
 *           AudioService after boot.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct AudioProfile {
    MixerState mixer;     ///< Input and stereo-mixer gains.
    EqProfile eq;         ///< Six-band parametric EQ.
    GainDb masterLeft;    ///< Multiple 1 master volume, left.
    GainDb masterRight;   ///< Multiple 1 master volume, right.

    /**
     * @brief    factoryDefault — factory-flat audio path (0 dB everywhere).
     *
     * @dname    factoryDefault
     * @return   AudioProfile suitable for first boot and reset.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static AudioProfile factoryDefault() noexcept;
};

} // namespace core
