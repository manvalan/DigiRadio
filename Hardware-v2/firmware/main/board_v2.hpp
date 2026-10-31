/**
 * @file    board_v2.hpp
 * @brief   v2 board power domain and the control buses that span it.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include "driver/i2c_master.h"

namespace board_v2 {

/**
 * @brief    enableAnalog — switch 3V3_AUDIO on with every audio pin floating.
 *
 * @dname    enableAnalog
 * @return   true when the rail was commanded on.
 * @pubstate drives ENABLE_ANALOG; holds SMODE low for the Si4684.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] bool enableAnalog();

/**
 * @brief    disableAnalog — release every audio pin, then drop 3V3_AUDIO.
 *
 * @dname    disableAnalog
 * @pubstate leaves the audio pins high impedance. A driven pin would feed
 *           the unpowered chips through their protection diodes.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
void disableAnalog();

/**
 * @brief    initBuses — create the control SPI bus and the I2C bus.
 *
 * @dname    initBuses
 * @return   true when both exist; repeat calls do nothing.
 * @pubstate owns SPI2_HOST and the I2C master bus for the process lifetime.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] bool initBuses();

/**
 * @brief    i2cBus — borrow the I2C master bus.
 *
 * @dname    i2cBus
 * @return   Bus handle, or nullptr before initBuses().
 * @pubstate reads the handle.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] i2c_master_bus_handle_t i2cBus() noexcept;

} // namespace board_v2
