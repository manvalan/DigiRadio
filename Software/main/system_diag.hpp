/**
 * @file    system_diag.hpp
 * @brief   Runtime diagnostics (reset reason, heap, task stacks) and the
 *          delayed OTA confirmation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string>

namespace system_diag {

/**
 * @brief    watchTask — include a task's stack low-water mark in reports.
 *
 * @dname    watchTask
 * @param    name    Short name for the JSON key (static storage).
 * @param    handle  Task handle; nullptr is ignored. The task must never
 *                   delete itself: reports read its stack through the
 *                   handle, and a freed one crashes the reader.
 * @pubstate appends to a fixed-size registry (silently full at 12).
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void watchTask(const char* name, TaskHandle_t handle);

/**
 * @brief    json — the "system" object of GET /api/health.
 *
 * @dname    json
 * @return   JSON text built by core::serializeSystemDiagnosticsJson().
 * @pubstate reads heap, reset reason, partition and registered tasks.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::string json();

/**
 * @brief    logSummary — one log line with the same figures.
 *
 * @dname    logSummary
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void logSummary();

/**
 * @brief    confirmOtaAfterProbation — keep a fresh OTA image only once it
 *           has run for a while.
 *
 * The confirmation used to happen right after the network came up, before
 * the af_follow and web radio tasks even started, so the 0.9.6 image that
 * crashed in af_follow was already "valid" and the bootloader had nothing
 * to roll back to. Now a timer confirms after kProbationSeconds; a reset
 * before that makes the bootloader return to the previous image.
 *
 * @dname    confirmOtaAfterProbation
 * @pubstate starts a one-shot esp_timer.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void confirmOtaAfterProbation();

} // namespace system_diag
