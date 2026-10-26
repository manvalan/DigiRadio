/**
 * @file    SystemDiagnostics.hpp
 * @brief   Runtime health figures (reset reason, heap, stacks) for
 *          GET /api/health.
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
 * @date    2026-10-26
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace core {

/** Least free stack ever seen for one task. */
struct TaskStackReport {
    std::string name;               ///< Task name as reported in JSON.
    std::uint32_t minFreeBytes{0U}; ///< uxTaskGetStackHighWaterMark().
};

/**
 * @brief    SystemDiagnostics — what goes wrong quietly on a device that
 *           has no serial cable attached.
 *
 * The 0.9.6 crash loop (an af_follow stack overflow) could only be seen
 * with a USB cable. The reset reason and the stack/heap low-water marks
 * make the same failure visible from the app.
 *
 * @dname    SystemDiagnostics
 * @return   n/a (type)
 * @pubstate Plain DTO filled by the shell, serialised by the core.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct SystemDiagnostics {
    std::string_view resetReason;      ///< e.g. "power_on", "panic".
    std::uint32_t uptimeSeconds{0U};   ///< Seconds since boot.
    std::uint32_t heapFreeBytes{0U};   ///< Internal heap free now.
    std::uint32_t heapMinFreeBytes{0U}; ///< Lowest since boot.
    std::uint32_t heapLargestBlockBytes{0U}; ///< Largest internal block.
    std::string_view runningPartition; ///< "ota_0" / "ota_1".
    bool otaPendingVerify{false};      ///< New image not yet confirmed.
    std::string_view si4684Store{"unknown"}; ///< Si4684ImageStore state token.
    bool si4684ImagesEmbedded{true};   ///< This build links the images in.
    std::vector<TaskStackReport> tasks; ///< Watched tasks' stacks.
};

/**
 * @brief    serializeSystemDiagnosticsJson — the "system" object of
 *           GET /api/health.
 *
 * @dname    serializeSystemDiagnosticsJson
 * @param    diag  Figures collected by the shell.
 * @return   JSON object string.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string serializeSystemDiagnosticsJson(
    const SystemDiagnostics& diag);

} // namespace core
