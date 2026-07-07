/**
 * @file    Adau1701Driver.hpp
 * @brief   ADAU1701 SigmaDSP driver — RAM boot and runtime safeload control.
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

#include "adau1701/Adau1701Error.hpp"

#include "core/AudioProfile.hpp"
#include "core/EqBandIndex.hpp"
#include "core/EqProfile.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"
#include "core/IDspProgramSource.hpp"
#include "core/MixSource.hpp"
#include "core/MixerState.hpp"

#include <cstdint>
#include <expected>

namespace adau1701 {

/**
 * @brief    Adau1701Pins — board GPIO/I2C identifiers for the DSP.
 *
 * @dname    Adau1701Pins
 * @return   n/a (type)
 * @pubstate Immutable wiring snapshot from board_pins.hpp.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct Adau1701Pins {
    int i2cSda;    ///< I2C SDA GPIO.
    int i2cScl;    ///< I2C SCL GPIO.
    int resetGpio; ///< DSP RESET# GPIO (active low).
    int i2cAddr7;  ///< 7-bit I2C address of the ADAU1701.
};

/**
 * @brief    Adau1701Driver — owns I2C + reset, boot, and safeload runtime.
 *
 * @dname    Adau1701Driver
 * @param    pins           Board wiring for I2C and RESET#.
 * @param    programSource  RAM download script (flash with embedded fallback).
 * @return   n/a (type)
 * @pubstate Owns I2C bus/device handles (RAII). booted_ true after a
 *           successful program replay.
 *
 * Writes the SigmaStudio export from Firmware/ADAU1701-Firmware on every
 * boot (no EEPROM self-boot on DigiRadio). Runtime EQ and mixer changes
 * use the ADAU1701 safeload mechanism.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Adau1701Driver {
public:
    /**
     * @brief    Adau1701Driver — construct with board pin map and program source.
     *
     * @dname    Adau1701Driver
     * @param    pins           SDA/SCL/reset/address configuration.
     * @param    programSource  Download script provider for boot().
     * @pubstate stores pins_ and programSource_; not booted until boot().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Adau1701Driver(Adau1701Pins pins,
                              core::IDspProgramSource& programSource);

    /**
     * @brief    ~Adau1701Driver — release I2C resources.
     *
     * @dname    ~Adau1701Driver
     * @pubstate deletes bus/device handles when created.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~Adau1701Driver();

    Adau1701Driver(const Adau1701Driver&) = delete;
    Adau1701Driver& operator=(const Adau1701Driver&) = delete;

    /**
     * @brief    boot — reset the DSP and replay the SigmaStudio download.
     *
     * @dname    boot
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes booted_ on success; uses embedded program data.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> boot();

    /**
     * @brief    isBooted — query whether RAM download succeeded.
     *
     * @dname    isBooted
     * @return   true after a successful boot().
     * @pubstate reads booted_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] bool isBooted() const noexcept;

    /**
     * @brief    i2cBusHandle — borrow the shared I2C master bus after boot.
     *
     * @dname    i2cBusHandle
     * @return   Opaque bus handle for the 24AA025E48 on the same bus, or null
     *           before boot().
     * @pubstate reads i2cBus_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] void* i2cBusHandle() const noexcept;

    /**
     * @brief    applyProfile — safeload mixer, EQ, and master from snapshot.
     *
     * @dname    applyProfile
     * @param    profile  User audio configuration.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> applyProfile(
        const core::AudioProfile& profile);

    /**
     * @brief    applyMixer — safeload input and stereo-mixer gains.
     *
     * @dname    applyMixer
     * @param    mixer  Per-source and St Mixer1 levels.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> applyMixer(
        const core::MixerState& mixer);

    /**
     * @brief    applyEq — safeload all six PEQ bands.
     *
     * @dname    applyEq
     * @param    eq  Six-band parametric EQ settings.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> applyEq(
        const core::EqProfile& eq);

    /**
     * @brief    setInputVolume — safeload one input path gain.
     *
     * @dname    setInputVolume
     * @param    source  Si4684 or ESP32 I2S path.
     * @param    left    Left channel gain.
     * @param    right   Right channel gain.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> setInputVolume(
        core::MixSource source, core::GainDb left, core::GainDb right);

    /**
     * @brief    setMasterVolume — safeload Multiple 1 master output gain.
     *
     * @dname    setMasterVolume
     * @param    left   Left master gain.
     * @param    right  Right master gain.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes parameter RAM via safeload.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> setMasterVolume(
        core::GainDb left, core::GainDb right);

    /**
     * @brief    setEqBand — design and safeload one PEQ band.
     *
     * @dname    setEqBand
     * @param    band   Band index 0..5.
     * @param    gain   Band gain in dB.
     * @param    center Centre frequency.
     * @param    q      Quality factor.
     * @return   Ok on success, or Adau1701Error.
     * @pubstate writes five coefficients in one safeload transfer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, Adau1701Error> setEqBand(
        core::EqBandIndex band, core::GainDb gain, core::FrequencyHz center,
        float q);

private:
    [[nodiscard]] std::expected<void, Adau1701Error> ensureBooted() const;
    [[nodiscard]] std::expected<void, Adau1701Error> safeloadGain(
        unsigned paramAddr, core::GainDb gain);
    [[nodiscard]] std::expected<void, Adau1701Error> safeloadFixpoint(
        unsigned paramAddr, std::int32_t fixpoint);
    [[nodiscard]] std::expected<void, Adau1701Error> replayProgram(
        const core::DspProgram& program);

    Adau1701Pins pins_;
    core::IDspProgramSource& programSource_;
    bool booted_;
    void* i2cBus_;
    void* i2cDev_;
};

} // namespace adau1701
