/**
 * @file    wireless.cpp
 * @brief   Wi-Fi and Bluetooth LE radio checks of the ESP32-S31 module.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * A scan proves the RF path and the PCB antenna of the module: the access
 * points and BLE advertisers around are listed with their RSSI.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/wireless.hpp"

#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

namespace bu {

namespace {

bool gNvs = false;
bool gWifi = false;
bool gBle = false;
SemaphoreHandle_t gBleSync = nullptr;
SemaphoreHandle_t gBleDone = nullptr;
std::atomic<int> gAdverts{0};
std::array<std::array<std::uint8_t, 6>, 64> gSeen = {};
std::atomic<int> gSeenCount{0};
std::atomic<int> gBestRssi{-127};

bool nvsInit()
{
    if (gNvs) {
        return true;
    }
    esp_err_t err = nvs_flash_init(); // PHY calibration data lives there
    if (err == ESP_ERR_NVS_NO_FREE_PAGES
        || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    gNvs = err == ESP_OK;
    return gNvs;
}

esp_err_t wifiInit()
{
    if (gWifi) {
        return ESP_OK;
    }
    if (!nvsInit()) {
        return ESP_FAIL;
    }
    esp_netif_init();
    const esp_err_t loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) {
        return loop;
    }
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err == ESP_OK) {
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        err = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (err == ESP_OK) {
        err = esp_wifi_start();
    }
    gWifi = err == ESP_OK;
    return err;
}

void onSync()
{
    xSemaphoreGive(gBleSync);
}

void hostTask(void* /*arg*/)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t bleInit()
{
    if (gBle) {
        return ESP_OK;
    }
    if (!nvsInit()) {
        return ESP_FAIL;
    }
    gBleSync = xSemaphoreCreateBinary();
    gBleDone = xSemaphoreCreateBinary();
    const esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        return err;
    }
    ble_hs_cfg.sync_cb = onSync;
    nimble_port_freertos_init(hostTask);
    if (xSemaphoreTake(gBleSync, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT; // controller never synced with the host
    }
    gBle = true;
    return ESP_OK;
}

void noteDevice(const ble_addr_t& addr, int rssi)
{
    gAdverts.fetch_add(1);
    gBestRssi.store(std::max(gBestRssi.load(), rssi));
    const int n = gSeenCount.load();
    for (int i = 0; i < n; ++i) {
        if (std::memcmp(gSeen[i].data(), addr.val, 6) == 0) {
            return;
        }
    }
    if (n < static_cast<int>(gSeen.size())) {
        std::memcpy(gSeen[n].data(), addr.val, 6);
        gSeenCount.store(n + 1);
    }
}

int gapEvent(struct ble_gap_event* event, void* /*arg*/)
{
    if (event->type == BLE_GAP_EVENT_DISC) {
        noteDevice(event->disc.addr, event->disc.rssi);
    } else if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        xSemaphoreGive(gBleDone);
    }
    return 0;
}

} // namespace

esp_err_t wifiScan(std::vector<WifiAp>& out)
{
    out.clear();
    if (esp_err_t err = wifiInit(); err != ESP_OK) {
        return err;
    }
    wifi_scan_config_t scan = {};
    scan.show_hidden = true;
    esp_err_t err = esp_wifi_scan_start(&scan, true);
    if (err != ESP_OK) {
        return err;
    }
    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    std::vector<wifi_ap_record_t> recs(count);
    err = esp_wifi_scan_get_ap_records(&count, recs.data());
    for (std::uint16_t i = 0; err == ESP_OK && i < count; ++i) {
        WifiAp ap;
        std::snprintf(ap.ssid, sizeof ap.ssid, "%s",
                      reinterpret_cast<const char*>(recs[i].ssid));
        ap.rssi = recs[i].rssi;
        ap.channel = recs[i].primary;
        out.push_back(ap);
    }
    return err;
}

esp_err_t wifiMac(std::uint8_t mac[6])
{
    return esp_read_mac(mac, ESP_MAC_WIFI_STA);
}

esp_err_t bleScan(int seconds, int& devices, int& adverts, int& bestRssi)
{
    devices = adverts = 0;
    bestRssi = -127;
    if (esp_err_t err = bleInit(); err != ESP_OK) {
        return err;
    }
    gAdverts.store(0);
    gSeenCount.store(0);
    gBestRssi.store(-127);
    uint8_t own = 0;
    if (ble_hs_id_infer_auto(0, &own) != 0) {
        return ESP_FAIL;
    }
    ble_gap_disc_params params = {};
    params.passive = 1;
    params.filter_duplicates = 0;
    xSemaphoreTake(gBleDone, 0);
    if (ble_gap_disc(own, seconds * 1000, &params, gapEvent, nullptr) != 0) {
        return ESP_FAIL;
    }
    xSemaphoreTake(gBleDone, pdMS_TO_TICKS(seconds * 1000 + 2000));
    devices = gSeenCount.load();
    adverts = gAdverts.load();
    bestRssi = gBestRssi.load();
    return ESP_OK;
}

} // namespace bu
