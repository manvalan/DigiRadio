/**
 * @file    BleProvisioning.cpp
 * @brief   BleProvisioning implementation.
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
 * @date    2026-08-18
 */

#include "net/BleProvisioning.hpp"

#include "core/Secret.hpp"
#include "core/WifiCredentials.hpp"
#include "core/WifiSsid.hpp"

#include "esp_log.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#include <cstring>
#include <optional>
#include <string>

namespace net::ble_provisioning {

namespace {
constexpr char kTag[] = "BleProvisioning";
constexpr unsigned kRebootDelaySec = 3;

// wifi_prov_mgr_init's callback is a plain C function pointer, so the store
// and the credentials received mid-handshake are kept in module statics —
// this mirrors the manager's own singleton lifetime (one instance for the
// life of the process, same as NetBootstrap's other network resources).
core::ISecureStore* gStore = nullptr;
std::optional<wifi_sta_config_t> gPendingCreds;

void rebootTask(void* arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(kRebootDelaySec * 1000));
    esp_restart();
}

/** wifi_sta_config_t::ssid/password are fixed-size byte arrays, zero-padded
 *  but not guaranteed null-terminated when exactly full length. */
[[nodiscard]] std::string_view boundedView(const std::uint8_t* bytes,
                                           std::size_t maxLen) noexcept
{
    const auto* chars = reinterpret_cast<const char*>(bytes);
    return std::string_view(chars, strnlen(chars, maxLen));
}

void saveAndReboot()
{
    if (gStore == nullptr || !gPendingCreds.has_value()) {
        ESP_LOGW(kTag, "credentials success event with no pending creds — "
                       "not saving");
        return;
    }

    const auto& cfg = *gPendingCreds;
    const std::string_view ssidRaw =
        boundedView(cfg.ssid, sizeof(cfg.ssid));
    const std::string_view passwordRaw =
        boundedView(cfg.password, sizeof(cfg.password));

    if (!core::WifiSsid::isValid(ssidRaw) ||
        !core::WifiCredentials::isPasswordValid(passwordRaw)) {
        ESP_LOGW(kTag, "BLE-provisioned credentials failed local validation "
                       "— not saving");
        return;
    }

    const core::WifiCredentials creds{core::WifiSsid(ssidRaw),
                                      core::Secret(std::string(passwordRaw))};
    if (!gStore->saveWifiCredentials(creds)) {
        ESP_LOGE(kTag, "failed to persist BLE-provisioned credentials");
        return;
    }

    ESP_LOGI(kTag, "Wi-Fi joined via BLE provisioning — rebooting into STA");
    xTaskCreate(rebootTask, "ble_prov_reboot", 2048, nullptr, 5, nullptr);
}

void provEventHandler(void* /*user_data*/, wifi_prov_cb_event_t event,
                      void* data)
{
    switch (event) {
    case WIFI_PROV_CRED_RECV:
        if (data != nullptr) {
            gPendingCreds = *static_cast<const wifi_sta_config_t*>(data);
            ESP_LOGI(kTag, "credentials received over BLE");
        }
        break;
    case WIFI_PROV_CRED_FAIL:
        ESP_LOGW(kTag, "BLE-provisioned Wi-Fi join failed — waiting for "
                       "the app to retry");
        gPendingCreds.reset();
        break;
    case WIFI_PROV_CRED_SUCCESS:
        saveAndReboot();
        break;
    default:
        break;
    }
}

} // namespace

std::expected<void, NetError>
start(core::ISecureStore& store, const core::DeviceIdentity& deviceIdentity)
{
    gStore = &store;
    gPendingCreds.reset();

    // Copy kept for the lifetime of provisioning: wifi_prov_mgr_start_
    // provisioning only borrows this pointer, it does not take ownership.
    static const std::string serviceName(deviceIdentity.softApSsid());

    const wifi_prov_mgr_config_t config{
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        .app_event_handler = {.event_cb = provEventHandler,
                              .user_data = nullptr},
    };
    if (wifi_prov_mgr_init(config) != ESP_OK) {
        ESP_LOGE(kTag, "wifi_prov_mgr_init failed");
        return std::unexpected(NetError::BleProvisioningFailed);
    }

    if (wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_0, nullptr,
                                         serviceName.c_str(),
                                         nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "wifi_prov_mgr_start_provisioning failed");
        wifi_prov_mgr_deinit();
        return std::unexpected(NetError::BleProvisioningFailed);
    }

    ESP_LOGI(kTag, "BLE provisioning advertising as %s (no PoP, same trust "
                   "level as the open SoftAP)",
             serviceName.c_str());
    return {};
}

} // namespace net::ble_provisioning
