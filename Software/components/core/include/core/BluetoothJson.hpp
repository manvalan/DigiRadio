/**
 * @file    BluetoothJson.hpp
 * @brief   JSON serialisation for Bluetooth REST API (pure core).
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

#include "core/Bt1035At.hpp"

#include <string>

namespace core {

/**
 * @brief    BluetoothStatus — snapshot for GET /api/bluetooth/status.
 *
 * @dname    BluetoothStatus
 * @return   n/a (type)
 * @pubstate Plain DTO assembled by BluetoothService.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct BluetoothStatus {
    bool booted;              ///< BT1035 driver ready after boot().
    bool pairing;             ///< Discoverable mode requested by firmware.
    Bt1035A2dpState a2dpState; ///< Last read A2DP link state.
};

/**
 * @brief    serializeBluetoothStatusJson — serialise BluetoothStatus for HTTP.
 *
 * @dname    serializeBluetoothStatusJson
 * @param    status  Domain snapshot.
 * @return   JSON object string.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeBluetoothStatusJson(
    const BluetoothStatus& status);

/**
 * @brief    serializeBluetoothErrorJson — serialise a Bluetooth API error.
 *
 * @dname    serializeBluetoothErrorJson
 * @param    reason  Short machine-readable cause.
 * @return   JSON error object.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::string serializeBluetoothErrorJson(const char* reason);

} // namespace core
