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

#include "net/BleProvisioning.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace net {

namespace {
constexpr char kTag[] = "StaClient";
constexpr int kConnectedBit = BIT0;
constexpr int kFailedBit = BIT1;
constexpr int kMaxConnectRetries = 10;
constexpr TickType_t kConnectTimeout = pdMS_TO_TICKS(45000);
constexpr TickType_t kRetryDelay = pdMS_TO_TICKS(800);
/** How long a post-boot link loss must persist before BLE fallback starts. */
constexpr TickType_t kBleFallbackThreshold = pdMS_TO_TICKS(60000);

EventGroupHandle_t s_wifiEventGroup = nullptr;
int s_connectRetries = 0;

// Set once by connect() on success when the caller opted in; used only by
// the post-boot branch of wifiEventHandler() below (s_wifiEventGroup is
// null there, since the bounded connect() call has already returned).
core::ISecureStore* s_bleFallbackStore = nullptr;
const core::DeviceIdentity* s_bleFallbackIdentity = nullptr;
TickType_t s_disconnectedSinceTick = 0;
bool s_bleFallbackActive = false;

/**
 * @brief    disconnectReasonString — map ESP-IDF Wi-Fi disconnect reason codes.
 *
 * @dname    disconnectReasonString
 * @param    reason  wifi_event_sta_disconnected_t::reason value.
 * @return   Short English label for logs.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] const char* disconnectReasonString(uint8_t reason) noexcept
{
    switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
        return "auth expired (wrong password?)";
    case WIFI_REASON_NO_AP_FOUND:
        return "no AP found (check SSID / 2.4 GHz)";
    case WIFI_REASON_AUTH_FAIL:
        return "auth failed (check password)";
    case WIFI_REASON_ASSOC_FAIL:
        return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "handshake timeout";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "beacon timeout (weak signal / PS)";
    case WIFI_REASON_CONNECTION_FAIL:
        return "connection failed";
    default:
        return "unknown";
    }
}

/**
 * @brief    applyStaLinkTuning — stabilise STA link after connect.
 *
 * @dname    applyStaLinkTuning
 * @pubstate Disables PS, forces 20 MHz, disables inactive disconnect.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
void applyStaLinkTuning() noexcept
{
    esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
    esp_wifi_set_inactive_time(WIFI_IF_STA, 0);
    ESP_LOGI(kTag, "STA link tuning applied (PS off, HT20, inactive off)");
}

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
    if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (eventBase == WIFI_EVENT
               && eventId == WIFI_EVENT_STA_DISCONNECTED) {
        const auto* disc =
            static_cast<const wifi_event_sta_disconnected_t*>(eventData);
        const uint8_t reason = disc != nullptr ? disc->reason : 0U;
        ESP_LOGW(kTag, "STA disconnected (reason %u: %s)",
                 static_cast<unsigned>(reason),
                 disconnectReasonString(reason));

        if (s_wifiEventGroup != nullptr) {
            if (s_connectRetries < kMaxConnectRetries) {
                ++s_connectRetries;
                ESP_LOGI(kTag, "retrying STA connect (%d/%d)",
                         s_connectRetries, kMaxConnectRetries);
                vTaskDelay(kRetryDelay);
                esp_wifi_connect();
                return;
            }
            xEventGroupSetBits(s_wifiEventGroup, kFailedBit);
            return;
        }

        // Post-boot link loss (the bounded connect() call above already
        // succeeded once): esp_wifi_connect() below retries forever on its
        // own, which used to be the whole story -- if the AP never comes
        // back (moved, replaced, password changed), the device retried
        // silently forever with no way for the app to reach it and no way
        // for the user to reconfigure Wi-Fi short of a power cycle. After
        // a sustained ~1 minute outage, start BLE provisioning alongside
        // the ongoing reconnect attempts (does not stop them) so the app
        // can push new credentials over Bluetooth.
        if (s_disconnectedSinceTick == 0) {
            s_disconnectedSinceTick = xTaskGetTickCount();
        } else if (!s_bleFallbackActive && s_bleFallbackStore != nullptr
                   && s_bleFallbackIdentity != nullptr
                   && (xTaskGetTickCount() - s_disconnectedSinceTick)
                          >= kBleFallbackThreshold) {
            ESP_LOGW(kTag, "STA link lost for over a minute — starting BLE "
                           "provisioning fallback");
            if (auto bleResult = ble_provisioning::start(
                    *s_bleFallbackStore, *s_bleFallbackIdentity);
                !bleResult) {
                ESP_LOGW(kTag, "BLE fallback provisioning failed to start");
            } else {
                s_bleFallbackActive = true;
            }
        }

        ESP_LOGW(kTag, "STA link lost — reconnecting");
        esp_wifi_connect();
    } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
        const auto* event =
            static_cast<const ip_event_got_ip_t*>(eventData);
        if (event != nullptr) {
            ESP_LOGI(kTag, "STA IP " IPSTR, IP2STR(&event->ip_info.ip));
        }
        applyStaLinkTuning();
        s_disconnectedSinceTick = 0;
        if (s_wifiEventGroup != nullptr) {
            xEventGroupSetBits(s_wifiEventGroup, kConnectedBit);
        }
    }
}

void unregisterStaHandlers(esp_event_handler_instance_t wifiHandler,
                             esp_event_handler_instance_t ipHandler) noexcept
{
    if (wifiHandler != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifiHandler);
    }
    if (ipHandler != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              ipHandler);
    }
}

} // namespace

StaClient::StaClient()
    : connected_(false)
    , wifiHandler_(nullptr)
    , ipHandler_(nullptr)
{
}

StaClient::~StaClient()
{
    unregisterStaHandlers(wifiHandler_, ipHandler_);
    wifiHandler_ = nullptr;
    ipHandler_ = nullptr;
    if (connected_) {
        esp_wifi_stop();
        connected_ = false;
    }
}

StaClient::StaClient(StaClient&& other) noexcept
    : connected_(other.connected_)
    , wifiHandler_(other.wifiHandler_)
    , ipHandler_(other.ipHandler_)
{
    other.connected_ = false;
    other.wifiHandler_ = nullptr;
    other.ipHandler_ = nullptr;
}

StaClient& StaClient::operator=(StaClient&& other) noexcept
{
    if (this != &other) {
        unregisterStaHandlers(wifiHandler_, ipHandler_);
        if (connected_) {
            esp_wifi_stop();
        }
        connected_ = other.connected_;
        wifiHandler_ = other.wifiHandler_;
        ipHandler_ = other.ipHandler_;
        other.connected_ = false;
        other.wifiHandler_ = nullptr;
        other.ipHandler_ = nullptr;
    }
    return *this;
}

std::expected<void, NetError>
StaClient::connect(const core::WifiCredentials& creds,
                   std::string_view hostname,
                   core::ISecureStore* bleFallbackStore,
                   const core::DeviceIdentity* bleFallbackIdentity)
{
    if (connected_) {
        return {};
    }

    s_bleFallbackStore = bleFallbackStore;
    s_bleFallbackIdentity = bleFallbackIdentity;
    s_disconnectedSinceTick = 0;
    s_bleFallbackActive = false;

    std::string hostLabel;
    if (!hostname.empty()) {
        hostLabel.assign(hostname.begin(), hostname.end());
    }

    s_wifiEventGroup = xEventGroupCreate();
    if (s_wifiEventGroup == nullptr) {
        return std::unexpected(NetError::StaConnectFailed);
    }
    s_connectRetries = 0;

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifiEventHandler,
                                        nullptr,
                                        &wifiHandler_);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifiEventHandler,
                                        nullptr,
                                        &ipHandler_);

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return std::unexpected(NetError::WifiConfigFailed);
    }

    if (!hostLabel.empty()) {
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif != nullptr) {
            esp_netif_set_hostname(netif, hostLabel.c_str());
        }
    }

    wifi_config_t wifiCfg = {};
    const std::string_view ssid = creds.ssid().value();
    const std::size_t ssidCopy =
        std::min(ssid.size(), sizeof(wifiCfg.sta.ssid) - 1U);
    std::memcpy(wifiCfg.sta.ssid, ssid.data(), ssidCopy);
    wifiCfg.sta.ssid[ssidCopy] = '\0';

    std::size_t pwdLen = 0U;
    creds.password().usePlaintext([&](std::string_view pwd) {
        pwdLen = pwd.size();
        const std::size_t pwdCopy =
            std::min(pwd.size(), sizeof(wifiCfg.sta.password) - 1U);
        std::memcpy(wifiCfg.sta.password, pwd.data(), pwdCopy);
        wifiCfg.sta.password[pwdCopy] = '\0';
    });

    if (pwdLen == 0U) {
        wifiCfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifiCfg.sta.threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK;
    }
    wifiCfg.sta.pmf_cfg.capable = true;
    wifiCfg.sta.pmf_cfg.required = false;
    wifiCfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifiCfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifiCfg.sta.failure_retry_cnt = 3;
    wifiCfg.sta.listen_interval = 1;

    if (esp_wifi_set_config(WIFI_IF_STA, &wifiCfg) != ESP_OK) {
        return std::unexpected(NetError::WifiConfigFailed);
    }

    if (esp_wifi_start() != ESP_OK) {
        return std::unexpected(NetError::WifiStartFailed);
    }

    esp_wifi_set_protocol(WIFI_IF_STA,
                          WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G
                              | WIFI_PROTOCOL_11N);
    esp_wifi_set_ps(WIFI_PS_NONE);

    const EventBits_t bits = xEventGroupWaitBits(s_wifiEventGroup,
                                                 kConnectedBit | kFailedBit,
                                                 pdTRUE,
                                                 pdFALSE,
                                                 kConnectTimeout);

    vEventGroupDelete(s_wifiEventGroup);
    s_wifiEventGroup = nullptr;

    if ((bits & kConnectedBit) != 0) {
        applyStaLinkTuning();
        connected_ = true;
        if (!hostLabel.empty()) {
            if (mdns_init() == ESP_OK) {
                mdns_hostname_set(hostLabel.c_str());
            }
        }
        ESP_LOGI(kTag, "connected to %.*s",
                 static_cast<int>(ssid.size()), ssid.data());
        return {};
    }

    unregisterStaHandlers(wifiHandler_, ipHandler_);
    wifiHandler_ = nullptr;
    ipHandler_ = nullptr;
    esp_wifi_stop();
    ESP_LOGW(kTag, "STA connect timed out or failed");
    if ((bits & kFailedBit) != 0) {
        return std::unexpected(NetError::StaConnectFailed);
    }
    return std::unexpected(NetError::StaConnectTimeout);
}

} // namespace net
