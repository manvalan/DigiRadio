/**
 * @file    StaClient.cpp
 * @brief   StaClient implementation.
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

#include "net/StaClient.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <algorithm>
#include <cstring>

namespace net {

namespace {
constexpr char kTag[] = "StaClient";
constexpr int kConnectedBit = BIT0;
constexpr int kFailedBit = BIT1;
constexpr TickType_t kConnectTimeout = pdMS_TO_TICKS(30000);

EventGroupHandle_t s_wifiEventGroup = nullptr;

/**
 * @brief    wifiEventHandler — signal connect success or failure.
 *
 * @dname    wifiEventHandler
 * @param    arg            Unused.
 * @param    eventBase      Event base identifier.
 * @param    eventId        Specific event id.
 * @param    eventData      Event payload.
 * @pubstate sets bits on s_wifiEventGroup.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
void wifiEventHandler(void* arg,
                      esp_event_base_t eventBase,
                      int32_t eventId,
                      void* eventData)
{
    (void)arg;
    (void)eventData;
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT
               && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_wifiEventGroup != nullptr) {
            xEventGroupSetBits(s_wifiEventGroup, kFailedBit);
        }
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        if (s_wifiEventGroup != nullptr) {
            xEventGroupSetBits(s_wifiEventGroup, kConnectedBit);
        }
    }
}
} // namespace

StaClient::StaClient()
    : connected_(false)
{
}

StaClient::~StaClient()
{
    if (connected_) {
        esp_wifi_stop();
        connected_ = false;
    }
}

StaClient::StaClient(StaClient&& other) noexcept
    : connected_(other.connected_)
{
    other.connected_ = false;
}

StaClient& StaClient::operator=(StaClient&& other) noexcept
{
    if (this != &other) {
        if (connected_) {
            esp_wifi_stop();
        }
        connected_ = other.connected_;
        other.connected_ = false;
    }
    return *this;
}

std::expected<void, NetError>
StaClient::connect(const core::WifiCredentials& creds)
{
    if (connected_) {
        return {};
    }

    s_wifiEventGroup = xEventGroupCreate();
    if (s_wifiEventGroup == nullptr) {
        return std::unexpected(NetError::StaConnectFailed);
    }

    esp_event_handler_instance_t instanceAnyId = nullptr;
    esp_event_handler_instance_t instanceGotIp = nullptr;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifiEventHandler,
                                        nullptr,
                                        &instanceAnyId);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifiEventHandler,
                                        nullptr,
                                        &instanceGotIp);

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return std::unexpected(NetError::WifiConfigFailed);
    }

    wifi_config_t wifiCfg = {};
    const std::string_view ssid = creds.ssid().value();
    const std::size_t ssidCopy =
        std::min(ssid.size(), sizeof(wifiCfg.sta.ssid) - 1);
    std::memcpy(wifiCfg.sta.ssid, ssid.data(), ssidCopy);

    creds.password().usePlaintext([&](std::string_view pwd) {
        const std::size_t pwdCopy =
            std::min(pwd.size(), sizeof(wifiCfg.sta.password) - 1);
        std::memcpy(wifiCfg.sta.password, pwd.data(), pwdCopy);
    });

    if (esp_wifi_set_config(WIFI_IF_STA, &wifiCfg) != ESP_OK) {
        return std::unexpected(NetError::WifiConfigFailed);
    }

    if (esp_wifi_start() != ESP_OK) {
        return std::unexpected(NetError::WifiStartFailed);
    }

    const EventBits_t bits = xEventGroupWaitBits(s_wifiEventGroup,
                                                 kConnectedBit | kFailedBit,
                                                 pdTRUE,
                                                 pdFALSE,
                                                 kConnectTimeout);

    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                          instanceGotIp);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                          instanceAnyId);
    vEventGroupDelete(s_wifiEventGroup);
    s_wifiEventGroup = nullptr;

    if ((bits & kConnectedBit) != 0) {
        connected_ = true;
        ESP_LOGI(kTag, "connected to %.*s",
                 static_cast<int>(ssid.size()), ssid.data());
        return {};
    }

    esp_wifi_stop();
    ESP_LOGW(kTag, "STA connect timed out or failed");
    if ((bits & kFailedBit) != 0) {
        return std::unexpected(NetError::StaConnectFailed);
    }
    return std::unexpected(NetError::StaConnectTimeout);
}

} // namespace net
