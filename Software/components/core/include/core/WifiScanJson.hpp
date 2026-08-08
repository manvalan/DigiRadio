/**
 * @file    WifiScanJson.hpp
 * @brief   JSON serialisation for Wi-Fi scan REST API (pure core).
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

#include <string>
#include <string_view>
#include <vector>

namespace core {

/**
 * @brief    serializeWifiScanJson — serialise scan result list.
 *
 * @dname    serializeWifiScanJson
 * @param    networks  Deduped AP records sorted by signal strength.
 * @return   JSON object with a networks array.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string serializeWifiScanJson(
    const std::vector<WifiScannedNetwork>& networks);

/**
 * @brief    serializeWifiScanErrorJson — serialise a Wi-Fi scan API error.
 *
 * @dname    serializeWifiScanErrorJson
 * @param    reason  Short machine-readable cause.
 * @return   JSON error object.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
[[nodiscard]] std::string serializeWifiScanErrorJson(std::string_view reason);

} // namespace core
