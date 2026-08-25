/**
 * @file    AudioService.hpp
 * @brief   Application service for ADAU1701 mixer/EQ/master configuration.
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
#include "core/IAudioProfileStore.hpp"
#include "core/IDsp.hpp"
#include "core/StoreError.hpp"

#include <expected>

namespace audio {

/**
 * @brief    AudioService — intent-level audio path control for HTTP/UI.
 *
 * @dname    AudioService
 * @return   n/a (type)
 * @pubstate Borrows core::IDsp; optionally persists via IAudioProfileStore.
 *           Tracks the last applied profile in RAM.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class AudioService {
public:
    /**
     * @brief    AudioService — bind DSP and optional profile store.
     *
     * @dname    AudioService
     * @param    dsp    ADAU1701 adapter (must outlive this service).
     * @param    store  Optional NVS store; nullptr skips persistence.
     * @pubstate initialises profile_ to factoryDefault().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    AudioService(core::IDsp& dsp, core::IAudioProfileStore* store);

    /**
     * @brief    loadAndApply — restore saved profile or factory default.
     *
     * @dname    loadAndApply
     * @return   true if a saved profile was restored from the store, false
     *          if none was found and AudioProfile::factoryDefault() was
     *          applied instead; DspError from IDsp on safeload failure.
     * @pubstate updates profile_ and safeloads the ADAU1701.
     *
     * Call once after NVS init (and ADAU1701 boot). Requires NVS to already
     * be initialized -- see the 2026-08-24 fix in main.cpp's app_main(),
     * which moved secure_store::initEncryptedStorage() ahead of
     * HardwareBootstrap::boot() specifically so this call can see a
     * previously-saved profile instead of always silently falling back to
     * default. Callers that want a sensible mixer/master fallback when
     * nothing was saved (see HardwareBootstrap::boot()'s
     * applyRadioFirstMix() call) should check the returned bool rather than
     * unconditionally overwriting whatever this restored.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<bool, core::DspError> loadAndApply();

    /**
     * @brief    currentProfile — read the in-memory profile snapshot.
     *
     * @dname    currentProfile
     * @return   Last applied or pending profile.
     * @pubstate reads profile_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::AudioProfile& currentProfile() const noexcept;

    /**
     * @brief    applyProfile — safeload a full profile and optionally save.
     *
     * @dname    applyProfile
     * @param    profile  Validated user configuration.
     * @param    persist  When true and store is set, write NVS.
     * @return   Ok on success, DspError, or StoreError::IoFailed.
     * @pubstate updates profile_; may persist via store_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> applyProfile(
        const core::AudioProfile& profile, bool persist);

    /**
     * @brief    selectSource — switch MX1's active input pair and apply live.
     *
     * @dname    selectSource
     * @param    source  Which stereo pair MX1 should pass through.
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError wrapping DspError/IoFailed.
     * @pubstate updates profile_.activeSource and safeloads.
     *
     * @author   Michele Bigi
     * @date     2026-08-25
     */
    [[nodiscard]] std::expected<void, core::StoreError> selectSource(
        core::ActiveSource source, bool persist);

    /**
     * @brief    setMasterVolume — update master output and apply live.
     *
     * @dname    setMasterVolume
     * @param    left    Left master gain.
     * @param    right   Right master gain.
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError.
     * @pubstate updates profile_ master fields and safeloads.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> setMasterVolume(
        core::GainDb left, core::GainDb right, bool persist);

    /**
     * @brief    applyRadioFirstMix — route Si4684 to output, mute ESP32 path.
     *
     * @dname    applyRadioFirstMix
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError.
     * @pubstate updates profile_.mixer and master; safeloads ADAU1701.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] std::expected<void, core::StoreError> applyRadioFirstMix(
        bool persist);

    /**
     * @brief    setEqBand — update one PEQ band and apply live.
     *
     * @dname    setEqBand
     * @param    band    Band index 0..5.
     * @param    gain    Band gain in dB.
     * @param    center  Centre frequency.
     * @param    q       Quality factor.
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError.
     * @pubstate updates profile_.eq and safeloads.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
        float q, bool persist);

    /**
     * @brief    setStereoEnhance — adjust stereo depth overlay (PEQ bands 3–5).
     *
     * @dname    setStereoEnhance
     * @param    level   Intensity 0..100 (0 = off).
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError.
     * @pubstate updates profile_.enhancements.stereo and safeloads effective EQ.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> setStereoEnhance(
        core::EnhanceLevel level, bool persist);

    /**
     * @brief    setBassEnhance — adjust bass emphasis overlay (PEQ bands 1–2).
     *
     * @dname    setBassEnhance
     * @param    level   Intensity 0..100 (0 = off).
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError.
     * @pubstate updates profile_.enhancements.bass and safeloads effective EQ.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> setBassEnhance(
        core::EnhanceLevel level, bool persist);

    /**
     * @brief    setBeepEnabled — toggle the ADAU1701 Beep1 tone generator.
     *
     * @dname    setBeepEnabled
     * @param    enabled  true unmutes Beep1, false mutes it.
     * @return   Ok on success, or DspError.
     * @pubstate live-only: not part of AudioProfile, never persisted, does
     *           not touch profile_.
     *
     * @author   Michele Bigi
     * @date     2026-08-07
     */
    [[nodiscard]] std::expected<void, core::DspError> setBeepEnabled(
        bool enabled);

    /**
     * @brief    writeRawParam — safeload any named ADAU1701 Parameter RAM cell.
     *
     * @dname    writeRawParam
     * @param    address  Parameter RAM address.
     * @param    value    Coefficient in the SigmaStudio floating convention.
     * @return   Ok on success, or DspError.
     * @pubstate live-only: not part of AudioProfile, never persisted, does
     *           not touch profile_. No domain validation.
     *
     * @author   Michele Bigi
     * @date     2026-08-18
     */
    [[nodiscard]] std::expected<void, core::DspError> writeRawParam(
        unsigned address, float value);

private:
    [[nodiscard]] std::expected<void, core::StoreError> persistProfile() const;

    [[nodiscard]] std::expected<void, core::StoreError> applyProfileToDsp(
        const core::AudioProfile& profile);

    core::IDsp& dsp_;
    core::IAudioProfileStore* store_;
    core::AudioProfile profile_;
};

} // namespace audio
