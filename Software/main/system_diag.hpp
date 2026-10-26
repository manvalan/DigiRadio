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
 * to roll back to. Since 0.12.4 a periodic check confirms only after 60 s
 * AND once a client has read GET /api/health after that (Wi-Fi, lwIP and
 * HTTP work) AND the heap never fell below 4 KB. 0.12.3 hung out of
 * memory, still confirmed by the old 60 s timer, and could not be rolled
 * back; an image that hangs or nearly runs out of memory now stays
 * unconfirmed, so the next reset returns to the previous image.
 *
 * @dname    confirmOtaAfterProbation
 * @pubstate starts a periodic esp_timer (5 s) that stops itself.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
void confirmOtaAfterProbation();

/**
 * @brief    bootSettled — whether background start-up may begin.
 *
 * @dname    bootSettled
 * @return   true once 20 s have passed since boot and the running image is
 *           confirmed (not in OTA probation).
 * @pubstate reads the uptime and the OTA state of the running partition.
 *
 * The web radio waits for this before resuming a stream that was left on,
 * so a fresh image is never judged while a stream eats the heap.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] bool bootSettled();

} // namespace system_diag
