/**
 * @file    wifi_scan_test.cpp
 * @brief   Host tests for Wi-Fi scan JSON serialisation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-05
 */

#include "core/WifiScanJson.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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
 * @date     2026-08-05
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
 * @brief    runWifiScanSerialiseTest — verify scan list JSON shape.
 *
 * @dname    runWifiScanSerialiseTest
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] int runWifiScanSerialiseTest()
{
    const std::vector<core::WifiScannedNetwork> networks = {
        {
            .ssid = "HomeNet",
            .rssiDbm = -42,
            .auth = "wpa2",
            .channel = 6,
        },
        {
            .ssid = "Guest\"WiFi",
            .rssiDbm = -71,
            .auth = "open",
            .channel = 11,
        },
    };

    const std::string json = core::serializeWifiScanJson(networks);
    const std::string expected =
        R"({"networks":[{"ssid":"HomeNet","rssi_dbm":-42,"auth":"wpa2","channel":6},{"ssid":"Guest\"WiFi","rssi_dbm":-71,"auth":"open","channel":11}]})";
    if (!expectEqual(json, expected)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief    runWifiScanErrorSerialiseTest — verify error JSON.
 *
 * @dname    runWifiScanErrorSerialiseTest
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] int runWifiScanErrorSerialiseTest()
{
    if (!expectEqual(core::serializeWifiScanErrorJson("scan_failed"),
                     R"({"status":"error","reason":"scan_failed"})")) {
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
 * @date     2026-08-05
 */
int main()
{
    if (runWifiScanSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runWifiScanErrorSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
