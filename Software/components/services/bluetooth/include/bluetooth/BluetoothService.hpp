/**
 * @file    BluetoothService.hpp
 * @brief   Intent-level Bluetooth API for HTTP and UI.
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

#include "bt1035/Bt1035Driver.hpp"
#include "bt1035/Bt1035Error.hpp"
#include "core/BluetoothJson.hpp"

#include <expected>
#include <vector>

namespace bluetooth {

/**
 * @brief    BluetoothService — pairing and A2DP status for the web API.
 *
 * @dname    BluetoothService
 * @return   n/a (type)
 * @pubstate Borrows bt1035::Bt1035Driver for the process lifetime. Tracks
 *           whether discoverable mode was requested via startPairing().
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class BluetoothService {
public:
    /**
     * @brief    BluetoothService — bind to the BT1035 driver.
     *
     * @dname    BluetoothService
     * @param    driver  Booted BT1035 driver (must outlive this service).
     * @pubstate stores driver reference; pairing inactive initially.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit BluetoothService(bt1035::Bt1035Driver& driver);

    /**
     * @brief    refreshStatus — read module boot, pairing, and A2DP state.
     *
     * @dname    refreshStatus
     * @return   BluetoothStatus on success, or Bt1035Error from the driver.
     * @pubstate queries driver for A2DP state when booted.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::BluetoothStatus, bt1035::Bt1035Error>
    refreshStatus();

    /**
     * @brief    startPairing — enter discoverable mode (AT+PAIR=1).
     *
     * @dname    startPairing
     * @return   Ok on success, or Bt1035Error.
     * @pubstate sets pairingActive_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> startPairing();

    /**
     * @brief    stopPairing — leave discoverable mode (AT+PAIR=0).
     *
     * @dname    stopPairing
     * @return   Ok on success, or Bt1035Error.
     * @pubstate clears pairingActive_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> stopPairing();

    /**
     * @brief    disconnect — release the active A2DP link (AT+A2DPDISC).
     *
     * @dname    disconnect
     * @return   Ok on success, or Bt1035Error.
     * @pubstate delegates to driver.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> disconnect();

    /**
     * @brief    listPaired — read paired remotes from the module.
     *
     * @dname    listPaired
     * @return   Device list on success, or Bt1035Error.
     * @pubstate queries AT+PLIST via the driver.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<std::vector<core::Bt1035PairedDevice>,
                                 bt1035::Bt1035Error>
    listPaired();

    /**
     * @brief    setAutoReconnect — configure power-on reconnect attempts.
     *
     * @dname    setAutoReconnect
     * @param    times  0 off, 1–15 per Feasycom manual.
     * @return   Ok on success, or Bt1035Error.
     * @pubstate writes AT+AUTOCONN via the driver.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> setAutoReconnect(
        std::uint8_t times);

private:
    bt1035::Bt1035Driver& driver_;
    bool pairingActive_;
};

} // namespace bluetooth
