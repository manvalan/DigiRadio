/**
 * @file    health_status_test.cpp
 * @brief   Host tests for HealthStatus JSON serialisation.
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

#include "core/FirmwareVersion.hpp"
#include "core/HealthStatus.hpp"
#include "core/HealthStatusJson.hpp"

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
 * @brief    runHealthStatusJsonTest — verify nominal JSON output.
 *
 * @dname    runHealthStatusJsonTest
 * @param    none
 * @return   EXIT_SUCCESS or EXIT_FAILURE.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] int runHealthStatusJsonTest()
{
    const core::HealthStatus status =
        core::HealthStatus::ok(core::FirmwareVersion("0.1.0"));
    const std::string json = core::serializeHealthStatusJson(status);
    if (!expectEqual(json, R"({"status":"ok","fw":"0.1.0"})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

/**
 * @brief    main — host test entry point.
 *
 * @dname    main
 * @param    none
 * @return   EXIT_SUCCESS when all tests pass.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
int main()
{
    return runHealthStatusJsonTest();
}
