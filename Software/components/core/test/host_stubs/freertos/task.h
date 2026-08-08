/**
 * @file    task.h
 * @brief   Host-test stand-in for the FreeRTOS task API.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Not shipped to device firmware: idf.py builds resolve the real FreeRTOS
 * headers first. vTaskDelay is a no-op here — host tests assert scan logic
 * against a FakeTuner, not real hardware settle timing.
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"

inline void vTaskDelay(TickType_t) {}
