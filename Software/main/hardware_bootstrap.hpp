/**
 * @file    hardware_bootstrap.hpp
 * @brief   Boot Si4684 and ADAU1701 companion chips at application start.
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

#include <expected>

namespace audio {
class AudioService;
} // namespace audio

namespace si4684 {
class Si4684Tuner;
} // namespace si4684

namespace hardware {

/**
 * @brief    HardwareBootError — companion-chip boot failure causes.
 *
 * @dname    HardwareBootError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class HardwareBootError {
    Si4684BootFailed,
    Adau1701BootFailed,
    Bt1035BootFailed,
};

/**
 * @brief    HardwareBootstrap — orchestrates Si4684 then ADAU1701 bring-up.
 *
 * @dname    HardwareBootstrap
 * @return   n/a (type)
 * @pubstate Owns static driver instances for the process lifetime. Must be
 *           called once from app_main before network start.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class HardwareBootstrap {
public:
    /**
     * @brief    boot — load Si4684 DAB image then replay ADAU1701 program.
     *
     * @dname    boot
     * @return   Ok on success, or HardwareBootError.
     * @pubstate constructs static Si4684, ADAU1701, and BT1035 drivers on first call.
     *
     * Fail-closed: callers must not start Wi-Fi when this returns an error.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<void, HardwareBootError> boot();

    /**
     * @brief    si4684Tuner — borrow the Si4684 ITuner adapter after boot.
     *
     * @dname    si4684Tuner
     * @return   Reference to the static Si4684Tuner instance.
     * @pubstate reads static storage initialised by boot().
     *
     * Valid only after a successful boot() call in the same process.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static si4684::Si4684Tuner& si4684Tuner();

    /**
     * @brief    audioService — borrow the audio orchestration service after boot.
     *
     * @dname    audioService
     * @return   Reference to the static AudioService instance.
     * @pubstate reads static storage initialised by boot().
     *
     * Valid only after a successful boot() call in the same process.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static audio::AudioService& audioService();
};

} // namespace hardware
