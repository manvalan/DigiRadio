/**
 * @file    FreeRTOS.h
 * @brief   Host-test stand-in for the FreeRTOS core header.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Not shipped to device firmware: idf.py builds resolve the real FreeRTOS
 * headers first. Only used so services/tuner/src/TunerService.cpp — which
 * paces scan steps with vTaskDelay on real hardware — can also compile
 * against components/core/test host tests without pulling in FreeRTOS.
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

using TickType_t = unsigned int;

#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
