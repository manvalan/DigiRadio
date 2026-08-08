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
#include "core/BtSpeakerTarget.hpp"
#include "core/ISecureStore.hpp"
#include "core/StoreError.hpp"

#include <expected>
#include <vector>

namespace bluetooth {

/**
 * @brief    BluetoothService — pairing, scan, and saved-speaker reconnect.
 *
 * @dname    BluetoothService
 * @return   n/a (type)
 * @pubstate Borrows bt1035::Bt1035Driver and core::ISecureStore for life.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class BluetoothService {
public:
    /**
     * @brief    BluetoothService — bind driver and store for the process life.
     *
     * @dname    BluetoothService
     * @param    driver  BT1035 driver (must outlive this service).
     * @param    store   Secure store for the saved default speaker.
     * @pubstate initialises pairingActive_ to false.
     */
    explicit BluetoothService(bt1035::Bt1035Driver& driver,
                              core::ISecureStore& store);

    /**
     * @brief    refreshStatus — read boot/pairing/name/reconnect/A2DP state.
     *
     * @dname    refreshStatus
     * @return   BluetoothStatus (booted=false if the module never booted),
     *           or a Bt1035Error from the first failing query.
     * @pubstate queries driver_; reads pairingActive_.
     */
    [[nodiscard]] std::expected<core::BluetoothStatus, bt1035::Bt1035Error>
    refreshStatus();

    /**
     * @brief    startPairing — enter BT1035 discoverable mode.
     *
     * @dname    startPairing
     * @return   Ok on success, or a Bt1035Error.
     * @pubstate sets pairingActive_ true on success.
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> startPairing();

    /**
     * @brief    stopPairing — leave BT1035 discoverable mode.
     *
     * @dname    stopPairing
     * @return   Ok on success, or a Bt1035Error.
     * @pubstate sets pairingActive_ false on success.
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> stopPairing();

    /**
     * @brief    disconnect — tear down the current A2DP link.
     *
     * @dname    disconnect
     * @return   Ok on success, or a Bt1035Error.
     * @pubstate none
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> disconnect();

    /**
     * @brief    listPaired — read the BT1035's paired-device list.
     *
     * @dname    listPaired
     * @return   Paired devices, or a Bt1035Error.
     * @pubstate none
     */
    [[nodiscard]] std::expected<std::vector<core::Bt1035PairedDevice>,
                                 bt1035::Bt1035Error>
    listPaired();

    /**
     * @brief    setAutoReconnect — set the module's auto-reconnect attempts.
     *
     * @dname    setAutoReconnect
     * @param    times  Retry count passed to the BT1035 firmware.
     * @return   Ok on success, or a Bt1035Error.
     * @pubstate none
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> setAutoReconnect(
        std::uint8_t times);

    /**
     * @brief    scanNearby — classic BT/EDR discovery, cancelling pairing.
     *
     * @dname    scanNearby
     * @param    scanSeconds  Scan duration in seconds.
     * @return   Discovered devices, or a Bt1035Error.
     * @pubstate leaves pairing mode and stops any active scan first; sets
     *           pairingActive_ false.
     */
    [[nodiscard]] std::expected<std::vector<core::Bt1035ScannedDevice>,
                                 bt1035::Bt1035Error>
    scanNearby(std::uint8_t scanSeconds = 20U);

    /**
     * @brief    connectTo — direct A2DPCONN to a MAC, wait for streaming.
     *
     * @dname    connectTo
     * @param    mac  Target BT1035 MAC (12 hex chars, no separators).
     * @return   Ok once A2DP reaches Streaming, or a Bt1035Error.
     * @pubstate sets pairingActive_ false; blocks up to the connect and
     *           stream settle timeouts.
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> connectTo(
        std::string_view mac);

    /**
     * @brief    connectTo — connect by request, optionally saving the target.
     *
     * @dname    connectTo
     * @param    request  Validated MAC/name plus a save flag.
     * @return   Ok once A2DP reaches Streaming, or a Bt1035Error
     *           (UnexpectedResponse for an invalid MAC).
     * @pubstate sets pairingActive_ false; persists via saveSpeaker() when
     *           request.save is true (a failed save does not fail connect).
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> connectTo(
        const core::BluetoothConnectRequest& request);

    /**
     * @brief    hasSavedSpeaker — check whether a default speaker is stored.
     *
     * @dname    hasSavedSpeaker
     * @return   true when loadSavedSpeaker() would succeed.
     * @pubstate reads store_.
     */
    [[nodiscard]] bool hasSavedSpeaker() const;

    /**
     * @brief    loadSavedSpeaker — read the stored default speaker.
     *
     * @dname    loadSavedSpeaker
     * @return   BtSpeakerTarget on success, or a StoreError.
     * @pubstate reads store_.
     */
    [[nodiscard]] std::expected<core::BtSpeakerTarget, core::StoreError>
    loadSavedSpeaker() const;

    /**
     * @brief    saveSpeaker — persist the default A2DP speaker target.
     *
     * @dname    saveSpeaker
     * @param    target  MAC and optional display name.
     * @return   Ok on success, or a StoreError.
     * @pubstate writes store_.
     */
    [[nodiscard]] std::expected<void, core::StoreError> saveSpeaker(
        const core::BtSpeakerTarget& target);

    /**
     * @brief    clearSavedSpeaker — erase the stored default speaker.
     *
     * @dname    clearSavedSpeaker
     * @return   Ok on success, or a StoreError.
     * @pubstate writes store_.
     */
    [[nodiscard]] std::expected<void, core::StoreError> clearSavedSpeaker();

    /** @brief Launch background task: A2DPCONN saved MAC, scan fallback. */
    void startupReconnect();

    /**
     * @brief    reconnectSavedSpeaker — direct MAC connect, optional scan retry.
     *
     * @dname    reconnectSavedSpeaker
     * @param    scanFallback  Run AT+SCAN when direct connect fails.
     * @return   Ok when A2DP reaches Connected, or Bt1035Error.
     * @pubstate may block tens of seconds when scanFallback is true.
     */
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> reconnectSavedSpeaker(
        bool scanFallback);

private:
    [[nodiscard]] std::expected<void, bt1035::Bt1035Error> connectDirect(
        std::string_view mac);

    bt1035::Bt1035Driver& driver_;
    core::ISecureStore& store_;
    bool pairingActive_;
};

} // namespace bluetooth
