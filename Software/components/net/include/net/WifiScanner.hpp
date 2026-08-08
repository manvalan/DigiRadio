/**
 * @file    WifiScanner.hpp
 * @brief   Blocking Wi-Fi scan wrapper for the setup web UI.
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

#include "core/WifiScannedNetwork.hpp"
#include "net/NetError.hpp"

#include <expected>
#include <vector>

namespace net {

/**
 * @brief    WifiScanner — runs esp_wifi scan and maps results to core DTOs.
 *
 * @dname    WifiScanner
 * @return   n/a (type)
 * @pubstate Stateless shell helper; assumes esp_wifi_init() already ran.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
class WifiScanner {
public:
    /**
     * @brief    scanNearby — list visible access points sorted by RSSI.
     *
     * @dname    scanNearby
     * @return   Deduped networks on success, or NetError::WifiScanFailed.
     * @pubstate Temporarily switches AP-only mode to APSTA when required.
     *
     * @author   Michele Bigi
     * @date     2026-08-05
     */
    [[nodiscard]] static std::expected<std::vector<core::WifiScannedNetwork>,
                                       NetError>
    scanNearby();
};

} // namespace net
