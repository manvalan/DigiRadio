/**
 * @file    IDsp.hpp
 * @brief   Abstract ADAU1701 DSP control boundary (host-testable).
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

#include "core/AudioProfile.hpp"
#include "core/DspError.hpp"
#include "core/EqBandIndex.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"
#include "core/MixSource.hpp"
#include "core/MixerState.hpp"
#include "core/EqProfile.hpp"

#include <expected>

namespace core {

/**
 * @brief    IDsp — hardware abstraction for ADAU1701 runtime control.
 *
 * @dname    IDsp
 * @return   n/a (type)
 * @pubstate Implemented by adau1701::Adau1701Dsp on device; fakes in tests.
 *
 * All parameter updates use safeload on the implementation side.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class IDsp {
public:
    virtual ~IDsp() = default;

    /**
     * @brief    applyProfile — safeload mixer, EQ, and master from a snapshot.
     *
     * @dname    applyProfile
     * @param    profile  Validated user configuration.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> applyProfile(
        const AudioProfile& profile) = 0;

    /**
     * @brief    applyMixer — safeload input and stereo-mixer gains.
     *
     * @dname    applyMixer
     * @param    mixer  Per-source and St Mixer1 levels.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> applyMixer(
        const MixerState& mixer) = 0;

    /**
     * @brief    applyEq — safeload all six PEQ bands.
     *
     * @dname    applyEq
     * @param    eq  Six-band parametric EQ settings.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> applyEq(
        const EqProfile& eq) = 0;

    /**
     * @brief    setInputVolume — safeload one input path (Si4684 or ESP32).
     *
     * @dname    setInputVolume
     * @param    source  Tuner or ESP32 I2S path.
     * @param    left    Left channel gain.
     * @param    right   Right channel gain.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> setInputVolume(
        MixSource source, GainDb left, GainDb right) = 0;

    /**
     * @brief    setMasterVolume — safeload Multiple 1 master output gain.
     *
     * @dname    setMasterVolume
     * @param    left   Left master gain.
     * @param    right  Right master gain.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> setMasterVolume(
        GainDb left, GainDb right) = 0;

    /**
     * @brief    setEqBand — design and safeload one PEQ band.
     *
     * @dname    setEqBand
     * @param    band   Band index 0..5.
     * @param    gain   Band gain in dB.
     * @param    center Centre frequency.
     * @param    q      Quality factor.
     * @return   Ok on success, or DspError.
     * @pubstate writes five coefficients via a single safeload transfer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, DspError> setEqBand(
        EqBandIndex band, GainDb gain, FrequencyHz center, float q) = 0;
};

} // namespace core
