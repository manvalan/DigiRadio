/**
 * @file    WifiScanner.cpp
 * @brief   WifiScanner implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-05
 */

#include "net/WifiScanner.hpp"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

namespace net {

namespace {
constexpr char kTag[] = "WifiScanner";

/**
 * @brief    authToken — map ESP-IDF auth mode to a short API token.
 *
 * @dname    authToken
 * @param    auth  wifi_ap_record_t::authmode value.
 * @return   Stable lowercase token for JSON.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] const char* authToken(wifi_auth_mode_t auth) noexcept
{
    switch (auth) {
    case WIFI_AUTH_OPEN:
        return "open";
    case WIFI_AUTH_WEP:
        return "wep";
    case WIFI_AUTH_WPA_PSK:
        return "wpa";
    case WIFI_AUTH_WPA2_PSK:
        return "wpa2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "wpa_wpa2";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "wpa2_enterprise";
    case WIFI_AUTH_WPA3_PSK:
        return "wpa3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "wpa2_wpa3";
    case WIFI_AUTH_WAPI_PSK:
        return "wapi";
    default:
        return "unknown";
    }
}

/**
 * @brief    ssidFromRecord — read a possibly unterminated SSID field.
 *
 * @dname    ssidFromRecord
 * @param    record  Raw ESP-IDF scan entry.
 * @return   SSID string (empty when hidden).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string ssidFromRecord(const wifi_ap_record_t& record)
{
    const std::size_t len =
        strnlen(reinterpret_cast<const char*>(record.ssid), sizeof(record.ssid));
    return std::string(reinterpret_cast<const char*>(record.ssid), len);
}

/**
 * @brief    ensureStaNetif — create default STA netif when missing.
 *
 * @dname    ensureStaNetif
 * @return   true when STA netif exists or was created.
 * @pubstate may call esp_netif_create_default_wifi_sta().
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] bool ensureStaNetif() noexcept
{
    if (esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") != nullptr) {
        return true;
    }
    return esp_netif_create_default_wifi_sta() != nullptr;
}

/**
 * @brief    dedupeBySsid — keep strongest RSSI per SSID.
 *
 * @dname    dedupeBySsid
 * @param    records  Raw scan rows from esp_wifi.
 * @return   Sorted networks, strongest first.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::vector<core::WifiScannedNetwork>
dedupeBySsid(const std::vector<wifi_ap_record_t>& records)
{
    std::unordered_map<std::string, core::WifiScannedNetwork> best;
    best.reserve(records.size());

    for (const wifi_ap_record_t& record : records) {
        const std::string ssid = ssidFromRecord(record);
        if (ssid.empty()) {
            continue;
        }

        core::WifiScannedNetwork entry = {
            .ssid = ssid,
            .rssiDbm = record.rssi,
            .auth = authToken(record.authmode),
            .channel = record.primary,
        };

        const auto existing = best.find(ssid);
        if (existing == best.end() || entry.rssiDbm > existing->second.rssiDbm) {
            best.emplace(ssid, std::move(entry));
        }
    }

    std::vector<core::WifiScannedNetwork> networks;
    networks.reserve(best.size());
    for (auto& item : best) {
        networks.push_back(std::move(item.second));
    }

    std::sort(networks.begin(), networks.end(),
              [](const core::WifiScannedNetwork& left,
                 const core::WifiScannedNetwork& right) {
                  return left.rssiDbm > right.rssiDbm;
              });
    return networks;
}

} // namespace

std::expected<std::vector<core::WifiScannedNetwork>, NetError>
WifiScanner::scanNearby()
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_get_mode failed");
        return std::unexpected(NetError::WifiScanFailed);
    }

    if (mode == WIFI_MODE_AP) {
        if (!ensureStaNetif()) {
            ESP_LOGE(kTag, "STA netif creation failed");
            return std::unexpected(NetError::WifiScanFailed);
        }
        wifi_config_t staCfg = {};
        if (esp_wifi_set_config(WIFI_IF_STA, &staCfg) != ESP_OK) {
            ESP_LOGE(kTag, "STA config for scan failed");
            return std::unexpected(NetError::WifiScanFailed);
        }
        if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) {
            ESP_LOGE(kTag, "APSTA mode switch failed");
            return std::unexpected(NetError::WifiScanFailed);
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    (void)esp_wifi_scan_stop();
    (void)esp_wifi_clear_ap_list();

    wifi_scan_config_t scanCfg = {};
    scanCfg.channel = 0;
    scanCfg.show_hidden = true;
    scanCfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scanCfg.scan_time.active.min = 300U;
    scanCfg.scan_time.active.max = 1200U;

    const esp_err_t scanErr = esp_wifi_scan_start(&scanCfg, true);
    if (scanErr != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_scan_start failed (%d)", static_cast<int>(scanErr));
        return std::unexpected(NetError::WifiScanFailed);
    }

    std::uint16_t count = 0U;
    if (esp_wifi_scan_get_ap_num(&count) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_scan_get_ap_num failed");
        return std::unexpected(NetError::WifiScanFailed);
    }

    std::vector<wifi_ap_record_t> records(count);
    if (count > 0U
        && esp_wifi_scan_get_ap_records(&count, records.data()) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_scan_get_ap_records failed");
        return std::unexpected(NetError::WifiScanFailed);
    }
    records.resize(count);

    ESP_LOGI(kTag, "raw AP count %u", static_cast<unsigned>(count));
    const auto networks = dedupeBySsid(records);
    ESP_LOGI(kTag, "scan found %u unique network(s)",
             static_cast<unsigned>(networks.size()));
    return networks;
}

} // namespace net
