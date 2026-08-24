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
#include "core/Bt1035PairedDevice.hpp"
#include "core/Bt1035ScannedDevice.hpp"
#include "core/BtSpeakerTarget.hpp"

#include <cstdint>
#include <string>
#include <vector>

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
    std::string deviceName;   ///< GAP friendly name from AT+NAME.
    std::uint8_t autoReconnect; ///< Power-on reconnect count (0 = off).
};

/**
 * @brief    BluetoothConnectRequest — POST /api/bluetooth/connect body.
 *
 * @dname    BluetoothConnectRequest
 * @return   n/a (type)
 * @pubstate Parsed by parseBluetoothConnectJson().
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct BluetoothConnectRequest {
    std::string mac;  ///< 12-char module MAC.
    std::string name; ///< Optional label stored when save is true.
    bool save;        ///< Persist as default speaker on success.
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

/**
 * @brief    serializeBluetoothPairedJson — serialise paired-device list.
 *
 * @dname    serializeBluetoothPairedJson
 * @param    devices  Parsed AT+PLIST entries.
 * @return   JSON object with a devices array.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::string serializeBluetoothPairedJson(
    const std::vector<Bt1035PairedDevice>& devices);

/**
 * @brief    parseBluetoothAutoReconnectJson — validate POST body times field.
 *
 * @dname    parseBluetoothAutoReconnectJson
 * @param    json  Request body with \c times 0–15.
 * @return   Reconnect count on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
[[nodiscard]] std::expected<std::uint8_t, ParseError>
parseBluetoothAutoReconnectJson(std::string_view json);

/**
 * @brief    parseBluetoothA2dpCodecConfigJson — validate POST codec_mask field.
 *
 * @dname    parseBluetoothA2dpCodecConfigJson
 * @param    json  Request body with \c codec_mask 0–63 (see AT+A2DPCFG bits).
 * @return   Bitmask on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-24
 */
[[nodiscard]] std::expected<std::uint8_t, ParseError>
parseBluetoothA2dpCodecConfigJson(std::string_view json);

/**
 * @brief    serializeBluetoothA2dpCodecJson — negotiated codec for HTTP.
 *
 * @dname    serializeBluetoothA2dpCodecJson
 * @param    codec  Parsed negotiated codec (AT+A2DPENC reply).
 * @return   JSON object \c {"codec":"..."}.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-24
 */
[[nodiscard]] std::string serializeBluetoothA2dpCodecJson(
    Bt1035A2dpCodec codec);

/**
 * @brief    serializeBluetoothScanJson — serialise scan result list.
 *
 * @dname    serializeBluetoothScanJson
 * @param    devices  Parsed +SCAN entries.
 * @return   JSON object with a devices array.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string serializeBluetoothScanJson(
    const std::vector<Bt1035ScannedDevice>& devices);

/**
 * @brief    parseBluetoothConnectJson — validate POST /api/bluetooth/connect.
 *
 * @dname    parseBluetoothConnectJson
 * @param    json  Request body with 12-char \c mac field.
 * @return   Normalised uppercase MAC on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::expected<std::string, ParseError>
parseBluetoothConnectJson(std::string_view json);

/**
 * @brief    parseBluetoothConnectRequest — mac, optional name, save flag.
 *
 * @dname    parseBluetoothConnectRequest
 * @param    json  POST /api/bluetooth/connect body.
 * @return   Parsed connect request (mac may be empty if invalid).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] BluetoothConnectRequest parseBluetoothConnectRequest(
    std::string_view json);

/**
 * @brief    serializeBluetoothSpeakerJson — saved default speaker for HTTP.
 *
 * @dname    serializeBluetoothSpeakerJson
 * @param    target  Stored speaker, or nullptr when not configured.
 * @return   JSON object with configured flag.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string serializeBluetoothSpeakerJson(
    const BtSpeakerTarget* target);

/**
 * @brief    parseBluetoothSpeakerJson — validate POST /api/bluetooth/speaker.
 *
 * @dname    parseBluetoothSpeakerJson
 * @param    json  Request body with mac and optional name.
 * @return   BtSpeakerTarget on success, or ParseError.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::expected<BtSpeakerTarget, ParseError>
parseBluetoothSpeakerJson(std::string_view json);

} // namespace core
