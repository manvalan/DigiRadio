/**
 * @file    system_diagnostics_test.cpp
 * @brief   Host tests for serializeSystemDiagnosticsJson.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/SystemDiagnostics.hpp"

#include <cstdlib>
#include <iostream>

int main()
{
    core::SystemDiagnostics diag;
    diag.resetReason = "panic";
    diag.uptimeSeconds = 61U;
    diag.heapFreeBytes = 120000U;
    diag.heapMinFreeBytes = 90000U;
    diag.heapLargestBlockBytes = 60000U;
    diag.runningPartition = "ota_1";
    diag.otaPendingVerify = true;
    diag.tasks = {{"af_follow", 5120U}, {"web_radio", 3000U}};
    const std::string json = core::serializeSystemDiagnosticsJson(diag);
    const std::string expected =
        R"({"reset_reason":"panic","uptime_s":61,"heap_free":120000,)"
        R"("heap_min_free":90000,"heap_largest_block":60000,)"
        R"("partition":"ota_1","ota_pending_verify":true,)"
        R"("stack_min_free":{"af_follow":5120,"web_radio":3000}})";
    if (json != expected) {
        std::cerr << "got " << json << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "system_diagnostics_test OK\n";
    return EXIT_SUCCESS;
}
