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

#include "core/AudioProfile.hpp"
#include "core/DspError.hpp"
#include "core/EqBandIndex.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"
#include "core/IAudioProfileStore.hpp"
#include "core/IDsp.hpp"
#include "core/MixSource.hpp"
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
     * @return   Ok on success, or DspError from IDsp.
     * @pubstate updates profile_ and safeloads the ADAU1701.
     *
     * Call once after ADAU1701 boot. When no profile is stored, applies
     * AudioProfile::factoryDefault() without writing NVS.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::DspError> loadAndApply();

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
     * @brief    setInputVolume — update one input path and apply live.
     *
     * @dname    setInputVolume
     * @param    source  Si4684 or ESP32 path.
     * @param    left    Left gain.
     * @param    right   Right gain.
     * @param    persist When true and store is set, write NVS.
     * @return   Ok on success, or StoreError wrapping DspError/IoFailed.
     * @pubstate updates profile_.mixer and safeloads.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> setInputVolume(
        core::MixSource source, core::GainDb left, core::GainDb right,
        bool persist);

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

private:
    [[nodiscard]] std::expected<void, core::StoreError> persistProfile() const;

    core::IDsp& dsp_;
    core::IAudioProfileStore* store_;
    core::AudioProfile profile_;
};

} // namespace audio
