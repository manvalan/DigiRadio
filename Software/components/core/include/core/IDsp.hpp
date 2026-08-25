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

#include "core/ActiveSource.hpp"
#include "core/AudioProfile.hpp"
#include "core/DspError.hpp"
#include "core/EnhanceLevel.hpp"
#include "core/EqBandIndex.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"
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
     * @brief    selectSource — safeload MX1's DC1 control to pick one source.
     *
     * @dname    selectSource
     * @param    source  Which stereo pair MX1 passes through to Param EQ1.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-08-25
     */
    [[nodiscard]] virtual std::expected<void, DspError> selectSource(
        ActiveSource source) = 0;

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

    /**
     * @brief    setBeepEnabled — gate the SigmaStudio Beep1 tone generator.
     *
     * @dname    setBeepEnabled
     * @param    enabled  true unmutes Beep1, false mutes it.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload. Not part of
     *           AudioProfile — live-only, never persisted.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    [[nodiscard]] virtual std::expected<void, DspError> setBeepEnabled(
        bool enabled) = 0;

    /**
     * @brief    setBassBoostLevel — scale Bass Boost1 toward its compiled
     *           curve.
     *
     * @dname    setBassBoostLevel
     * @param    level  0 = flat bypass, 100 = the full SigmaStudio-tuned
     *                  Dynamic Bass Boost response.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload. Linearly
     *           interpolates the compiled crossover filter and 33-point
     *           compander curve toward identity/unity; BASSFREQUENCY and
     *           the time constant are left at their tuned defaults.
     *
     * @author   Michele Bigi
     * @date     2026-08-25
     */
    [[nodiscard]] virtual std::expected<void, DspError> setBassBoostLevel(
        EnhanceLevel level) = 0;

    /**
     * @brief    setStereoSpreadLevel — scale SPhat1's stereo spread amount.
     *
     * @dname    setStereoSpreadLevel
     * @param    level  0 = no added spread, 100 = the full SigmaStudio-tuned
     *                  SuperPhat spread.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload. Scales
     *           SPREAD1/SPREAD2 linearly; the crossover filter and
     *           compander curve are left at their tuned defaults.
     *
     * @author   Michele Bigi
     * @date     2026-08-25
     */
    [[nodiscard]] virtual std::expected<void, DspError> setStereoSpreadLevel(
        EnhanceLevel level) = 0;

    /**
     * @brief    writeRawParam — safeload any named Parameter RAM cell.
     *
     * @dname    writeRawParam
     * @param    address  Parameter RAM address.
     * @param    value    Coefficient in the SigmaStudio floating
     *                    convention; converted to ADAU 8.23 fixpoint.
     * @return   Ok on success, or DspError.
     * @pubstate writes ADAU1701 parameter RAM via safeload. Not part of
     *           AudioProfile — live-only, never persisted. No domain
     *           validation; see Adau1701Driver::writeRawParam().
     *
     * @author   Michele Bigi
     * @date     2026-08-18
     */
    [[nodiscard]] virtual std::expected<void, DspError> writeRawParam(
        unsigned address, float value) = 0;
};

} // namespace core
