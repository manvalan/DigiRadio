/**
 * @file    Bt1035ScannedDevice.hpp
 * @brief   One entry from FSC-BT1035 AT+SCAN (+SCAN= lines).
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
 * @brief    Bt1035ScannedDevice — nearby remote from +SCAN= events.
 *
 * @dname    Bt1035ScannedDevice
 * @return   n/a (type)
 * @pubstate Plain DTO parsed in the pure core.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct Bt1035ScannedDevice {
    std::uint8_t index;       ///< Scan result index from the module.
    std::uint8_t addressType; ///< 0/1 LE, 2 BR/EDR (Feasycom manual).
    std::string mac;          ///< 12-char ASCII MAC without separators.
    std::int16_t rssiDbm;     ///< RSSI in dBm (-127..-1).
    std::string name;         ///< Remote friendly name when advertised.
    std::string deviceClass;  ///< Class of device hex string when present.
};

} // namespace core
