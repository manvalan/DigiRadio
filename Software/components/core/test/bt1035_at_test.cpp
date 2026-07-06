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
    return EXIT_SUCCESS;
}
