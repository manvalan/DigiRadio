/**
 * @file    wifi_provision_test.cpp
 * @brief   Host tests for Wi-Fi provisioning JSON parse/serialise.
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
 * @date    2026-07-06
 */

#include "core/ParseError.hpp"
#include "core/WifiCredentials.hpp"
#include "core/WifiProvisionJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/**
 * @brief    expectEqual — assert two strings match.
 *
 * @dname    expectEqual
 * @param    actual    Observed value.
 * @param    expected  Expected value.
 * @return   true when equal.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] bool expectEqual(const std::string& actual,
                               const std::string& expected)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << "expected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

/**
 * @brief    runWifiProvisionParseTest — verify nominal JSON parsing.
 *
 * @dname    runWifiProvisionParseTest
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] int runWifiProvisionParseTest()
{
    const auto parsed = core::parseWifiProvisionJson(
        R"({"ssid":"MyNet","password":"secret12"})");
    if (!parsed) {
        std::cerr << "parse failed\n";
        return EXIT_FAILURE;
    }
    if (parsed->ssid().value() != "MyNet") {
        std::cerr << "unexpected ssid\n";
        return EXIT_FAILURE;
    }
    bool pwdOk = false;
    parsed->password().usePlaintext(
        [&](std::string_view pwd) { pwdOk = (pwd == "secret12"); });
    if (!pwdOk) {
        std::cerr << "unexpected password\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief    runWifiProvisionRejectTest — verify invalid SSID is rejected.
 *
 * @dname    runWifiProvisionRejectTest
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] int runWifiProvisionRejectTest()
{
    const auto parsed =
        core::parseWifiProvisionJson(R"({"ssid":"","password":"secret12"})");
    if (parsed || parsed.error() != core::ParseError::InvalidSsid) {
        std::cerr << "expected InvalidSsid\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief    runWifiProvisionSerialiseTest — verify saved response JSON.
 *
 * @dname    runWifiProvisionSerialiseTest
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] int runWifiProvisionSerialiseTest()
{
    if (!expectEqual(core::serializeWifiProvisionSavedJson(3),
                     R"({"status":"saved","reboot_in_sec":3})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

/**
 * @brief    main — host test entry point.
 *
 * @dname    main
 * @return   EXIT_SUCCESS when all tests pass.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
int main()
{
    if (runWifiProvisionParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runWifiProvisionRejectTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runWifiProvisionSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
