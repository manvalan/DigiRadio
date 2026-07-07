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
    if (sequence[1U] != core::Bt1035AtCommand::AuxLineIn) {
        std::cerr << "AUXCFG=1 must be in init sequence\n";
        return EXIT_FAILURE;
    }
    const std::string aux = core::buildBt1035AtLine(core::Bt1035AtCommand::AuxLineIn);
    if (aux != "AT+AUXCFG=1\r\n") {
        std::cerr << "AUXCFG command line mismatch\n";
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
    return EXIT_SUCCESS;
}
