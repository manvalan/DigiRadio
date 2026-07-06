/**
 * @file    SoftApHost.cpp
 * @brief   SoftApHost implementation.
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

#include "net/SoftApHost.hpp"

#include "esp_wifi.h"
#include "esp_log.h"

#include <cstring>

namespace net {

namespace {
constexpr char kTag[] = "SoftApHost";
} // namespace

SoftApHost::SoftApHost(SoftApConfig config)
    : config_(config)
    , started_(false)
{
}

SoftApHost::~SoftApHost()
{
    if (started_) {
        esp_wifi_stop();
        started_ = false;
    }
}

SoftApHost::SoftApHost(SoftApHost&& other) noexcept
    : config_(other.config_)
    , started_(other.started_)
{
    other.started_ = false;
}

SoftApHost& SoftApHost::operator=(SoftApHost&& other) noexcept
{
    if (this != &other) {
        if (started_) {
            esp_wifi_stop();
        }
        config_ = other.config_;
        started_ = other.started_;
        other.started_ = false;
    }
    return *this;
}

std::expected<void, NetError> SoftApHost::start()
{
    if (started_) {
        return {};
    }

    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_mode failed");
        return std::unexpected(NetError::WifiConfigFailed);
    }

    wifi_config_t wifiCfg = {};
    const std::string_view ssid = config_.ssid();
    std::memcpy(wifiCfg.ap.ssid, ssid.data(), ssid.size());
    wifiCfg.ap.ssid_len = static_cast<int>(ssid.size());
    wifiCfg.ap.channel = config_.channel();
    wifiCfg.ap.max_connection = config_.maxConnections();
    wifiCfg.ap.authmode = WIFI_AUTH_OPEN;

    if (esp_wifi_set_config(WIFI_IF_AP, &wifiCfg) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_set_config failed");
        return std::unexpected(NetError::WifiConfigFailed);
    }

    if (esp_wifi_start() != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_start failed");
        return std::unexpected(NetError::WifiStartFailed);
    }

    started_ = true;
    ESP_LOGI(kTag, "SoftAP started: %.*s", static_cast<int>(ssid.size()),
             ssid.data());
    return {};
}

} // namespace net
