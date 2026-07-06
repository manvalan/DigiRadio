/**
 * @file    Adau1701Driver.hpp
 * @brief   ADAU1701 SigmaDSP driver — RAM boot on every power-up.
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
 * @brief    Adau1701Driver — owns I2C + reset and loads SigmaStudio RAM.
 *
 * @dname    Adau1701Driver
 * @param    pins  Board wiring for I2C and RESET#.
 * @return   n/a (type)
 * @pubstate Owns I2C bus/device handles (RAII). booted_ true after a
 *           successful default_download replay.
 *
 * Writes the SigmaStudio export from Firmware/ADAU1701-Firmware on every
 * boot (no EEPROM self-boot on DigiRadio).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Adau1701Driver {
public:
    /**
     * @brief    Adau1701Driver — construct with board pin map.
     *
     * @dname    Adau1701Driver
     * @param    pins  SDA/SCL/reset/address configuration.
     * @pubstate stores pins_; not booted until boot().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit Adau1701Driver(Adau1701Pins pins);

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

private:
    Adau1701Pins pins_;
    bool booted_;
    void* i2cBus_;
    void* i2cDev_;
};

} // namespace adau1701
