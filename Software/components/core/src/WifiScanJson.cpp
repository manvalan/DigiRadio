/**
 * @file    WifiScanJson.cpp
 * @brief   Wi-Fi scan JSON serialisation implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-05
 */

#include "core/WifiScanJson.hpp"

#include <sstream>

namespace core {

namespace {

/**
 * @brief    appendJsonString — emit a JSON string literal.
 *
 * @dname    appendJsonString
 * @param    out   Output stream.
 * @param    text  Raw UTF-8 text.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
void appendJsonString(std::ostringstream& out, std::string_view text)
{
    out << '"';
    for (const char ch : text) {
        if (ch == '"' || ch == '\\') {
            out << '\\';
        }
        out << ch;
    }
    out << '"';
}

} // namespace

std::string serializeWifiScanJson(
    const std::vector<WifiScannedNetwork>& networks)
{
    std::ostringstream out;
    out << "{\"networks\":[";
    for (std::size_t i = 0; i < networks.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const WifiScannedNetwork& network = networks[i];
        out << "{\"ssid\":";
        appendJsonString(out, network.ssid);
        out << ",\"rssi_dbm\":" << network.rssiDbm << ",\"auth\":";
        appendJsonString(out, network.auth);
        out << ",\"channel\":" << static_cast<unsigned>(network.channel) << '}';
    }
    out << "]}";
    return out.str();
}

std::string serializeWifiScanErrorJson(std::string_view reason)
{
    return std::string("{\"status\":\"error\",\"reason\":\"") + std::string(reason)
           + "\"}";
}

} // namespace core
