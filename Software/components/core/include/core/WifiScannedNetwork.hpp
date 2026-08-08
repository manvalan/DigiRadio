/**
 * @file    WifiScannedNetwork.hpp
 * @brief   One entry from a nearby Wi-Fi access-point scan.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-05
 */
#pragma once

#include <cstdint>
#include <string>

namespace core {

/**
 * @brief    WifiScannedNetwork — nearby AP from a Wi-Fi scan.
 *
 * @dname    WifiScannedNetwork
 * @return   n/a (type)
 * @pubstate Plain DTO assembled by the net shell after esp_wifi scan.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct WifiScannedNetwork {
    std::string ssid;       ///< Broadcast SSID (may be empty when hidden).
    std::int16_t rssiDbm;   ///< RSSI in dBm.
    std::string auth;         ///< Short auth token (open, wpa2, wpa3, …).
    std::uint8_t channel;   ///< Primary channel number.
};

} // namespace core
