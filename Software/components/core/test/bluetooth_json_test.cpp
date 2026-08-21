/**
 * @file    bluetooth_json_test.cpp
 * @brief   Host tests for Bluetooth JSON parse/serialise.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-19
 */

#include "core/BluetoothJson.hpp"
#include "core/ParseError.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool expectEqual(const std::string& actual,
                               const std::string& expected)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << "expected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

[[nodiscard]] int runStatusSerialiseTest()
{
    const core::BluetoothStatus status{
        .booted = true,
        .pairing = false,
        .a2dpState = core::Bt1035A2dpState::Streaming,
        .deviceName = "DigiRadio-CC4DB4",
        .autoReconnect = 3U,
    };
    const std::string json = core::serializeBluetoothStatusJson(status);
    if (!expectEqual(json,
                     R"({"booted":true,"pairing":false,"a2dp":"streaming",)"
                     R"("device_name":"DigiRadio-CC4DB4","auto_reconnect":3})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runScanSerialiseTest()
{
    const std::vector<core::Bt1035ScannedDevice> devices{
        core::Bt1035ScannedDevice{
            .index = 1U,
            .addressType = 2U,
            .mac = "001122334455",
            .rssiDbm = -58,
            .name = "Bose SoundLink",
            .deviceClass = "240404",
        },
    };
    const std::string json = core::serializeBluetoothScanJson(devices);
    if (!expectEqual(json,
                     R"({"devices":[{"index":1,"mac":"001122334455",)"
                     R"("name":"Bose SoundLink","rssi_dbm":-58}]})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runPairedSerialiseTest()
{
    const std::vector<core::Bt1035PairedDevice> devices{
        core::Bt1035PairedDevice{.index = 1U, .mac = "AABBCCDDEEFF", .name = "Phone"},
    };
    const std::string json = core::serializeBluetoothPairedJson(devices);
    if (!expectEqual(json,
                     R"({"devices":[{"index":1,"mac":"AABBCCDDEEFF","name":"Phone"}]})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runAutoReconnectParseTest()
{
    const auto ok = core::parseBluetoothAutoReconnectJson(R"({"times":5})");
    if (!ok || *ok != 5U) {
        std::cerr << "auto-reconnect valid parse failed\n";
        return EXIT_FAILURE;
    }
    const auto tooHigh = core::parseBluetoothAutoReconnectJson(R"({"times":16})");
    if (tooHigh) {
        std::cerr << "auto-reconnect out-of-range accepted\n";
        return EXIT_FAILURE;
    }
    const auto missing = core::parseBluetoothAutoReconnectJson(R"({})");
    if (missing || missing.error() != core::ParseError::MissingField) {
        std::cerr << "auto-reconnect missing field mis-reported\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runConnectJsonParseTest()
{
    const auto ok =
        core::parseBluetoothConnectJson(R"({"mac":"001122334455"})");
    if (!ok || *ok != "001122334455") {
        std::cerr << "connect mac parse failed\n";
        return EXIT_FAILURE;
    }
    // Normalises to uppercase.
    const auto lower =
        core::parseBluetoothConnectJson(R"({"mac":"aabbccddeeff"})");
    if (!lower || *lower != "AABBCCDDEEFF") {
        std::cerr << "connect mac uppercasing failed\n";
        return EXIT_FAILURE;
    }
    const auto invalid = core::parseBluetoothConnectJson(R"({"mac":"not-a-mac"})");
    if (invalid) {
        std::cerr << "connect invalid mac accepted\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runConnectRequestParseTest()
{
    const auto request = core::parseBluetoothConnectRequest(
        R"({"mac":"001122334455","name":"Bose SoundLink","save":true})");
    if (request.mac != "001122334455" || request.name != "Bose SoundLink"
        || !request.save) {
        std::cerr << "connect request full parse failed\n";
        return EXIT_FAILURE;
    }
    const auto minimal =
        core::parseBluetoothConnectRequest(R"({"mac":"001122334455"})");
    if (minimal.mac != "001122334455" || !minimal.name.empty()
        || minimal.save) {
        std::cerr << "connect request minimal parse failed\n";
        return EXIT_FAILURE;
    }
    const auto badMac = core::parseBluetoothConnectRequest(R"({})");
    if (!badMac.mac.empty()) {
        std::cerr << "connect request missing mac should stay empty\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runSpeakerRoundTripTest()
{
    const core::BtSpeakerTarget target{.mac = "001122334455",
                                       .name = "Bose SoundLink"};
    const std::string json = core::serializeBluetoothSpeakerJson(&target);
    if (!expectEqual(json,
                     R"({"configured":true,"mac":"001122334455",)"
                     R"("name":"Bose SoundLink"})")) {
        return EXIT_FAILURE;
    }
    const std::string unset = core::serializeBluetoothSpeakerJson(nullptr);
    if (!expectEqual(unset, R"({"configured":false})")) {
        return EXIT_FAILURE;
    }

    const auto parsed = core::parseBluetoothSpeakerJson(
        R"({"mac":"001122334455","name":"Bose SoundLink"})");
    if (!parsed || parsed->mac != "001122334455"
        || parsed->name != "Bose SoundLink") {
        std::cerr << "speaker parse round-trip failed\n";
        return EXIT_FAILURE;
    }
    const auto invalid = core::parseBluetoothSpeakerJson(R"({"mac":"bad"})");
    if (invalid) {
        std::cerr << "speaker parse invalid mac accepted\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runErrorSerialiseTest()
{
    const std::string json = core::serializeBluetoothErrorJson("scan_failed");
    if (!expectEqual(json, R"({"status":"error","reason":"scan_failed"})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runStatusSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runScanSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runPairedSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runAutoReconnectParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runConnectJsonParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runConnectRequestParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runSpeakerRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runErrorSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
