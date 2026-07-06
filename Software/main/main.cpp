/**
 * @file    main.cpp
 * @brief   DigiRadio imperative shell entry point (app_main).
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

#include "net/NetBootstrap.hpp"
#include "secure_store/NvsSecureStore.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "digiradio";
constexpr TickType_t kHeartbeatPeriod = pdMS_TO_TICKS(5000);

/**
 * @brief    heartbeatTask — periodic alive log for bring-up verification.
 *
 * @dname    heartbeatTask
 * @param    arg  Unused task parameter.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
void heartbeatTask(void* arg)
{
    (void)arg;
    while (true) {
        ESP_LOGI(kTag, "heartbeat");
        vTaskDelay(kHeartbeatPeriod);
    }
}

} // namespace

/**
 * @brief    app_main — ESP-IDF application entry point.
 *
 * @dname    app_main
 * @pubstate starts NvsSecureStore, NetBootstrap, and the heartbeat task.
 *
 * Slice 2: joins a stored Wi-Fi network when credentials exist, otherwise
 * opens the DigiRadio-setup SoftAP for provisioning via the web UI.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
extern "C" void app_main()
{
    ESP_LOGI(kTag, "DigiRadio firmware boot — Slice 2");

    static secure_store::NvsSecureStore store;

    auto netResult = net::NetBootstrap::start(store);
    if (!netResult) {
        ESP_LOGE(kTag, "network bootstrap failed");
        return;
    }

    static net::NetBootstrap net = std::move(netResult.value());

    if (xTaskCreate(heartbeatTask, "heartbeat", 2048, nullptr, 5, nullptr)
        != pdPASS) {
        ESP_LOGE(kTag, "heartbeat task create failed");
        return;
    }

    if (net.state() == net::NetState::StaConnected) {
        ESP_LOGI(kTag, "running in STA mode — open http://<device-ip>/");
    } else {
        ESP_LOGI(kTag,
                 "running in setup mode — join DigiRadio-setup, "
                 "open http://192.168.4.1/");
    }
}
