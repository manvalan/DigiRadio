/**
 * @file    BleProvisioning.hpp
 * @brief   BLE GATT Wi-Fi provisioning, additive alongside the setup SoftAP.
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
 * @date    2026-08-18
 */
#pragma once

#include "core/DeviceIdentity.hpp"
#include "core/ISecureStore.hpp"
#include "net/NetError.hpp"

#include <expected>

namespace net::ble_provisioning {

/**
 * @brief    start — start the ESP-IDF wifi_provisioning manager over BLE.
 *
 * @dname    start
 * @param    store           Secure store the received credentials are saved
 *                            to (same store POST /api/wifi writes to).
 * @param    deviceIdentity  Supplies the BLE advertising name (softApSsid)
 *                            and the proof-of-possession string (serialNumber).
 * @return   Ok once provisioning is advertising, or NetError::BleProvisioningFailed.
 * @pubstate Starts the onboard ESP32-S3 BLE radio (independent of the BT1035
 *           UART module) and a process-lifetime wifi_provisioning manager
 *           singleton. On a successful join, saves credentials to store and
 *           reboots, mirroring wifiPostHandler's POST /api/wifi behaviour.
 *           Runs alongside the existing SoftAP + HTTP provisioning route,
 *           not instead of it — either path can complete setup.
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
[[nodiscard]] std::expected<void, NetError>
start(core::ISecureStore& store, const core::DeviceIdentity& deviceIdentity);

} // namespace net::ble_provisioning
