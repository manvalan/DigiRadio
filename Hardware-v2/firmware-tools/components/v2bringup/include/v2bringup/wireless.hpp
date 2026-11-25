/**
 * @file    wireless.hpp
 * @brief   Wi-Fi and Bluetooth LE radio checks of the ESP32-S31 module.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */
#pragma once

#include "esp_err.h"

#include <cstdint>
#include <vector>

namespace bu {

struct WifiAp {
    char ssid[33] = {};
    int rssi = 0;
    int channel = 0;
};

/** Station mode scan of every channel (about 2-4 s). */
esp_err_t wifiScan(std::vector<WifiAp>& out);
esp_err_t wifiMac(std::uint8_t mac[6]);
/** Passive LE scan; distinct addresses, adverts received, best RSSI. */
esp_err_t bleScan(int seconds, int& devices, int& adverts, int& bestRssi);

} // namespace bu
