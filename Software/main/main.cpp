/**
 * @file    main.cpp
 * @brief   DigiRadio imperative shell entry point (app_main).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "hardware_bootstrap.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "integration/IntegrationService.hpp"
#include "net/NetBootstrap.hpp"
#include "ota/OtaService.hpp"
#include "secure_store/NvsSecureStore.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char kTag[] = "digiradio";
constexpr TickType_t kHeartbeatPeriod = pdMS_TO_TICKS(5000);

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
 * @brief    app_main — ESP-IDF entry point for DigiRadio firmware.
 *
 * @dname    app_main
 * @pubstate boots companion chips, integration layer, network, and heartbeat.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
extern "C" void app_main()
{
    ESP_LOGI(kTag, "DigiRadio firmware boot");

    auto hwResult = hardware::HardwareBootstrap::boot();
    if (!hwResult) {
        ESP_LOGE(kTag, "companion chip boot failed — halting");
        return;
    }

    static secure_store::NvsSecureStore store;

    static tuner::TunerService tunerService(
        hardware::HardwareBootstrap::si4684Tuner());

    static station::StationService stationService(store, tunerService);

    static integration::IntegrationService integration(
        store,
        tunerService,
        hardware::HardwareBootstrap::audioService(),
        stationService);

    if (auto started = integration.startup(); !started) {
        ESP_LOGW(kTag, "integration startup failed — continuing without recall");
    }

    static bluetooth::BluetoothService bluetoothService(
        hardware::HardwareBootstrap::bt1035Driver());

    static ota::OtaService otaService;

    auto netResult = net::NetBootstrap::start(
        store,
        tunerService,
        hardware::HardwareBootstrap::audioService(),
        bluetoothService,
        stationService,
        integration,
        otaService,
        hardware::HardwareBootstrap::companionChipStatus(),
        hardware::HardwareBootstrap::deviceIdentity());
    if (!netResult) {
        ESP_LOGE(kTag, "network bootstrap failed");
        return;
    }

    static net::NetBootstrap net = std::move(netResult.value());

    if (auto confirmed = ota::OtaService::confirmBoot(); !confirmed) {
        ESP_LOGW(kTag, "OTA rollback confirm failed");
    }

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
