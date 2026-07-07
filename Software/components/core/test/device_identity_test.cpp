/**
 * @file    device_identity_test.cpp
 * @brief   Host tests for Eui48 and DeviceIdentity derivation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/DeviceIdentity.hpp"
#include "core/Eui48.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool expectEqual(std::string_view actual,
                               std::string_view expected)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << "expected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

[[nodiscard]] int runKnownEuiTest()
{
    const std::array<std::uint8_t, 6> bytes = {
        0x00U, 0x04U, 0xA3U, 0x12U, 0x34U, 0x56U,
    };
    const core::Eui48 eui = core::Eui48::fromBytes(bytes);
    if (!expectEqual(eui.serialNumber(), "0004A3123456")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(eui.shortSuffix(), "123456")) {
        return EXIT_FAILURE;
    }

    const core::DeviceIdentity identity = core::DeviceIdentity::fromEui48(eui);
    if (!identity.isKnown()) {
        std::cerr << "expected known identity\n";
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.serialNumber(), "0004A3123456")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.softApSsid(), "DigiRadio-123456")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.bluetoothName(), "DigiRadio-123456")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.hostname(), "digiradio-123456")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runUnknownIdentityTest()
{
    const core::DeviceIdentity identity = core::DeviceIdentity::unknown();
    if (identity.isKnown()) {
        std::cerr << "expected unknown identity\n";
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.serialNumber(), "unknown")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.softApSsid(), "DigiRadio-setup")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.bluetoothName(), "DigiRadio")) {
        return EXIT_FAILURE;
    }
    if (!expectEqual(identity.hostname(), "digiradio")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runKnownEuiTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runUnknownIdentityTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
