/**
 * @file    BluetoothJson.cpp
 * @brief   BluetoothJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BluetoothJson.hpp"

#include <sstream>

namespace core {

std::string serializeBluetoothStatusJson(const BluetoothStatus& status)
{
    std::ostringstream out;
    out << "{\"booted\":" << (status.booted ? "true" : "false")
        << ",\"pairing\":" << (status.pairing ? "true" : "false")
        << ",\"a2dp\":\"" << a2dpStateToken(status.a2dpState) << "\"}";
    return out.str();
}

std::string serializeBluetoothErrorJson(const char* reason)
{
    std::ostringstream out;
    out << "{\"status\":\"error\",\"reason\":\"" << reason << "\"}";
    return out.str();
}

} // namespace core
