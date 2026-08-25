/**
 * @file    AudioLevels.hpp
 * @brief   Snapshot of the six ADAU1701 Data Capture level meters.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-25
 */
#pragma once

namespace core {

/**
 * @brief    AudioLevels — one on-demand read of all six 1×RTA meters.
 *
 * @dname    AudioLevels
 * @return   n/a (type)
 * @pubstate Read-only snapshot; not part of AudioProfile, never persisted.
 *
 * Each field is in dBFS, read live from the ADAU1701 Data Capture Register
 * (address 2074, MAC_out tap) at the program-step index SigmaStudio's
 * compiler assigned to that 1×RTA cell (see
 * Firmware/ADAU1701-Firmware/IC 1_DigiRadioFinale/net_list_out2/trap.dat).
 * Captured only when requested by an API call -- no background polling.
 *
 * @author   Michele Bigi
 * @date     2026-08-25
 */
struct AudioLevels {
    float radioInLeftDb;      ///< 1×RTA1, post-compressor Si4684 L.
    float radioInRightDb;     ///< 1×RTA2, post-compressor Si4684 R.
    float bluetoothInLeftDb;  ///< 1×RTA3, ESP32 L.
    float bluetoothInRightDb; ///< 1×RTA4, ESP32 R.
    float outputLeftDb;       ///< 1×RTA1_2, post Bass Boost1, L.
    float outputRightDb;      ///< 1×RTA2_2, post Bass Boost1, R.
};

} // namespace core
