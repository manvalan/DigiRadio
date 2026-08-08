/**
 * @file    esp_log.h
 * @brief   Host-test stand-in for ESP-IDF logging macros.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Not shipped to device firmware: idf.py builds resolve the real ESP-IDF
 * esp_log.h first. Only used so services/tuner/src/TunerService.cpp — which
 * logs scan progress on real hardware — can also compile against
 * components/core/test host tests without pulling in ESP-IDF.
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
