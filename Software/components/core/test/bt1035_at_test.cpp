/**
 * @file    bt1035_at_test.cpp
 * @brief   Host tests for FSC-BT1035 AT command builder and parser.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/Bt1035At.hpp"
#include "core/Bt1035PairedDevice.hpp"
#include "core/Bt1035ScannedDevice.hpp"
#include "core/BluetoothJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] int runInitSequenceTest()
{
    const auto sequence = core::bootInitSequence();
    if (sequence.size() != core::kBt1035BootInitCommandCount) {
        std::cerr << "init sequence size mismatch\n";
        return EXIT_FAILURE;
    }
    if (sequence[1U] != core::Bt1035AtCommand::I2sMode) {
        std::cerr << "I2S mode must be in init sequence\n";
        return EXIT_FAILURE;
    }
    if (sequence[2U] != core::Bt1035AtCommand::I2sSlave48k24) {
        std::cerr << "I2SCFG must be in init sequence\n";
        return EXIT_FAILURE;
    }
    const std::string i2sMode =
        core::buildBt1035AtLine(core::Bt1035AtCommand::I2sMode);
    if (i2sMode != "AT+AUXCFG=3\r\n") {
        std::cerr << "AUXCFG=3 command line mismatch\n";
        return EXIT_FAILURE;
    }
    const std::string i2sCfg =
        core::buildBt1035AtLine(core::Bt1035AtCommand::I2sSlave48k24);
    if (i2sCfg != "AT+I2SCFG=35\r\n") {
        std::cerr << "I2SCFG=35 command line mismatch\n";
        return EXIT_FAILURE;
    }
    const std::string reset =
        core::buildBt1035AtLine(core::Bt1035AtCommand::Reset);
    if (reset != "AT+RESET\r\n") {
        std::cerr << "AT+RESET command line mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseTest()
{
    if (core::parseBt1035AtResponse("OK\r\n") != core::Bt1035AtResponseKind::Ok) {
        std::cerr << "OK parse failed\n";
        return EXIT_FAILURE;
    }
    if (core::parseBt1035AtResponse("ERROR\r\n")
        != core::Bt1035AtResponseKind::Error) {
        std::cerr << "ERROR parse failed\n";
        return EXIT_FAILURE;
    }
    if (core::parseBt1035AtResponse("garbage")
        != core::Bt1035AtResponseKind::Unexpected) {
        std::cerr << "unexpected parse failed\n";
        return EXIT_FAILURE;
    }
    if (core::parseBt1035AtResponse("+A2DPSTAT=3\r\nOK\r\n")
        != core::Bt1035AtResponseKind::Ok) {
        std::cerr << "expected multiline OK detection\n";
        return EXIT_FAILURE;
    }
    const std::string pairOn =
        core::buildBt1035AtLine(core::Bt1035AtCommand::PairDiscoverable);
    if (pairOn != "AT+PAIR=1\r\n") {
        std::cerr << "expected AT+PAIR=1 line\n";
        return EXIT_FAILURE;
    }
    const auto a2dp =
        core::parseBt1035A2dpStatResponse("+A2DPSTAT=4\r\nOK\r\n");
    if (!a2dp || *a2dp != core::Bt1035A2dpState::Streaming) {
        std::cerr << "expected streaming A2DP state\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runNameAutoConnPairedParseTest()
{
    const auto name =
        core::parseBt1035NameResponse("+NAME=DigiRadio-A1B2\r\nOK\r\n");
    if (!name || *name != "DigiRadio-A1B2") {
        std::cerr << "NAME parse failed\n";
        return EXIT_FAILURE;
    }
    const auto autoconn =
        core::parseBt1035AutoConnResponse("+AUTOCONN=3\r\nOK\r\n");
    if (!autoconn || *autoconn != 3U) {
        std::cerr << "AUTOCONN parse failed\n";
        return EXIT_FAILURE;
    }
    const auto plist = core::parseBt1035PairedListResponse(
        "+PLIST=1,001122334455,Phone\r\n"
        "+PLIST=2,FFEEDDCCBBAA,\r\n"
        "OK\r\n");
    if (!plist || plist->size() != 2U || (*plist)[0U].index != 1U
        || (*plist)[0U].mac != "001122334455"
        || (*plist)[0U].name != "Phone") {
        std::cerr << "PLIST parse failed\n";
        return EXIT_FAILURE;
    }
    const std::string pairedJson =
        core::serializeBluetoothPairedJson(*plist);
    if (pairedJson.find("\"index\":1") == std::string::npos
        || pairedJson.find("\"mac\":\"001122334455\"") == std::string::npos) {
        std::cerr << "paired JSON serialise failed: " << pairedJson << '\n';
        return EXIT_FAILURE;
    }
    const auto times =
        core::parseBluetoothAutoReconnectJson(R"({"times":5})");
    if (!times || *times != 5U) {
        std::cerr << "auto-reconnect JSON parse failed\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035SetAutoConnLine(5) != "AT+AUTOCONN=5\r\n") {
        std::cerr << "AUTOCONN command line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035SetNameLine("DigiRadio-A1B2", false)
        != "AT+NAME=DigiRadio-A1B2,0\r\n") {
        std::cerr << "NAME set command line mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runScanConnectTest()
{
    if (core::buildBt1035StartScanLine(1U, 20U) != "AT+SCAN=1,20\r\n") {
        std::cerr << "SCAN start line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035StartScanLine(1U, 0U) != "AT+SCAN=1\r\n") {
        std::cerr << "SCAN default line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035StartScanLine(2U, 15U) != "AT+SCAN=2,15\r\n") {
        std::cerr << "BLE SCAN line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035EnablePrintLine() != "AT+PRINT=1\r\n") {
        std::cerr << "PRINT line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035DisableAutoLinkLine() != "AT+LINKCFG=0,0\r\n") {
        std::cerr << "LINKCFG line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035ClearPairedListLine() != "AT+PLIST=0\r\n") {
        std::cerr << "PLIST clear line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035DisconnectAllLine() != "AT+DSCA\r\n") {
        std::cerr << "DSCA line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035StopScanLine() != "AT+SCAN=0\r\n") {
        std::cerr << "SCAN stop line mismatch\n";
        return EXIT_FAILURE;
    }
    if (!core::isValidBt1035Mac("AABBCCDDEEFF")
        || core::isValidBt1035Mac("bad")) {
        std::cerr << "MAC validation failed\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035A2dpConnectLine("aabbccddeeff")
        != "AT+A2DPCONN=aabbccddeeff\r\n") {
        std::cerr << "A2DPCONN line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035A2dpAudioLine(true) != "AT+A2DPAUDIO=1\r\n") {
        std::cerr << "A2DPAUDIO=1 line mismatch\n";
        return EXIT_FAILURE;
    }
    if (core::buildBt1035A2dpAudioLine(false) != "AT+A2DPAUDIO=0\r\n") {
        std::cerr << "A2DPAUDIO=0 line mismatch\n";
        return EXIT_FAILURE;
    }
    const auto scanned = core::parseBt1035ScanResponse(
        "+SCAN=1,0,112233445566,-55,7,MyPhone,240404\r\n"
        "+SCAN=2,0,FFEEDDCCBBAA,-70,0,,240404\r\n"
        "+SCAN=E\r\n"
        "OK\r\n");
    if (!scanned || scanned->size() != 2U || (*scanned)[0U].mac != "112233445566"
        || (*scanned)[0U].name != "MyPhone" || (*scanned)[0U].rssiDbm != -55
        || !(*scanned)[1U].name.empty()) {
        std::cerr << "SCAN parse failed\n";
        return EXIT_FAILURE;
    }
    const auto colonMac = core::parseBt1035ScanResponse(
        "+SCAN=1,2,AA:BB:CC:DD:EE:FF,-60,4,Test,240404\r\n+SCAN=E\r\n");
    if (!colonMac || colonMac->size() != 1U
        || (*colonMac)[0U].mac != "AABBCCDDEEFF") {
        std::cerr << "SCAN colon MAC parse failed\n";
        return EXIT_FAILURE;
    }
    const auto userLogScan = core::parseBt1035ScanResponse(
        "+DEVSTAT=9\r\n\r\nOK\r\n\r\n"
        "+SCAN=1,2,6960BB7EC5AA,-71,8,HY300PRO,1A0114\r\n\r\n"
        "+SCAN=2,2,BC87FAE69D6E,-49,0\r\n\r\n"
        "+DEVSTAT=1\r\n");
    if (!userLogScan || userLogScan->size() != 2U
        || (*userLogScan)[0U].name != "HY300PRO"
        || (*userLogScan)[0U].mac != "6960BB7EC5AA"
        || (*userLogScan)[1U].mac != "BC87FAE69D6E"
        || !(*userLogScan)[1U].name.empty()
        || (*userLogScan)[1U].rssiDbm != -49) {
        std::cerr << "SCAN user-log payload parse failed\n";
        return EXIT_FAILURE;
    }
    const auto bareEmptyName = core::parseBt1035ScanResponse(
        "+SCAN=2,2,BC87FAE69D6E,-49,0\r\n");
    if (!bareEmptyName || bareEmptyName->size() != 1U
        || (*bareEmptyName)[0U].mac != "BC87FAE69D6E"
        || !(*bareEmptyName)[0U].name.empty()
        || !(*bareEmptyName)[0U].deviceClass.empty()) {
        std::cerr << "SCAN bare empty-name line parse failed\n";
        return EXIT_FAILURE;
    }
    const std::string scanJson =
        core::serializeBluetoothScanJson(*scanned);
    if (scanJson.find("\"mac\":\"112233445566\"") == std::string::npos
        || scanJson.find("\"rssi_dbm\":-55") == std::string::npos) {
        std::cerr << "scan JSON serialise failed: " << scanJson << '\n';
        return EXIT_FAILURE;
    }
    const auto mac =
        core::parseBluetoothConnectJson(R"({"mac":"112233445566"})");
    if (!mac || *mac != "112233445566") {
        std::cerr << "connect JSON parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runInitSequenceTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runNameAutoConnPairedParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runScanConnectTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
