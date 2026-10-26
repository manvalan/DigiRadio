/**
 * @file    SystemDiagnostics.cpp
 * @brief   Runtime health figures (reset reason, heap, stacks) for
 *          GET /api/health.
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

#include <sstream>

namespace core {

namespace {

void appendTasks(std::ostringstream& out,
                 const std::vector<TaskStackReport>& tasks)
{
    out << R"(,"stack_min_free":{)";
    bool first = true;
    for (const TaskStackReport& task : tasks) {
        out << (first ? "" : ",") << '"' << task.name
            << "\":" << task.minFreeBytes;
        first = false;
    }
    out << '}';
}

} // namespace

std::string serializeSystemDiagnosticsJson(const SystemDiagnostics& diag)
{
    std::ostringstream out;
    out << R"({"reset_reason":")" << diag.resetReason << '"'
        << R"(,"uptime_s":)" << diag.uptimeSeconds
        << R"(,"heap_free":)" << diag.heapFreeBytes
        << R"(,"heap_min_free":)" << diag.heapMinFreeBytes
        << R"(,"heap_largest_block":)" << diag.heapLargestBlockBytes
        << R"(,"partition":")" << diag.runningPartition << '"'
        << R"(,"ota_pending_verify":)"
        << (diag.otaPendingVerify ? "true" : "false");
    appendTasks(out, diag.tasks);
    out << '}';
    return out.str();
}

} // namespace core
