/**
 * @file    PowerState.hpp
 * @brief   User-facing power state of the radio (on / standby).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-21
 */
#pragma once

namespace core {

/**
 * @brief    PowerState — whether the radio is playing or in soft standby.
 *
 * @dname    PowerState
 * @return   n/a (type)
 * @pubstate n/a
 *
 * The board has no ESP32-controlled power switch (main/board_pins.hpp), so
 * "off" is a soft standby: the ADAU1701 master output is muted while the
 * ESP32, Wi-Fi and HTTP server stay up, so the app can switch it back on.
 * Standby is live-only: it is never persisted and every boot starts On.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
enum class PowerState {
    On,      ///< Normal operation: master output at the profile's level.
    Standby, ///< Master output muted; network stays reachable.
};

} // namespace core
