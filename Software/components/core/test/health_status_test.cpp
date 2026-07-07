/**
 * @file    health_status_test.cpp
 * @brief   Host tests for HealthStatus JSON serialisation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FirmwareVersion.hpp"
#include "core/CompanionChipStatus.hpp"
#include "core/HealthStatus.hpp"
#include "core/HealthStatusJson.hpp"

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

[[nodiscard]] int runHealthStatusJsonTest()
{
    const core::HealthStatus status =
        core::HealthStatus::ok(core::FirmwareVersion("0.1.0"));
    const std::string json = core::serializeHealthStatusJson(status);
    if (!expectEqual(json,
                     R"({"status":"ok","fw":"0.1.0","serialNumber":"unknown"})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runHealthStatusJsonTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    const core::HealthStatus withChips = core::HealthStatus::ok(
        core::FirmwareVersion("0.6.0"),
        core::CompanionChipStatus{
            .si4684Ready = true,
            .adau1701Ready = true,
            .bt1035Ready = true,
        },
        "0004A3123456");
    const std::string chipsJson = core::serializeHealthStatusJson(withChips);
    if (!expectEqual(
            chipsJson,
            R"({"status":"ok","fw":"0.6.0","serialNumber":"0004A3123456","chips":{"si4684":true,"adau1701":true,"bt1035":true}})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
