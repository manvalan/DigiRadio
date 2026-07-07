/**
 * @file    Bt1035PairedDevice.hpp
 * @brief   One entry from FSC-BT1035 AT+PLIST paired-record query.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include <cstdint>
#include <string>

namespace core {

/**
 * @brief    Bt1035PairedDevice — paired remote from +PLIST= lines.
 *
 * @dname    Bt1035PairedDevice
 * @return   n/a (type)
 * @pubstate Plain DTO parsed from Feasycom BT1035 AT responses.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
struct Bt1035PairedDevice {
    std::uint8_t index;  ///< Module paired-record index (1–8).
    std::string mac;     ///< 12-digit hex MAC without separators.
    std::string name;    ///< UTF-8 friendly name when supplied.
};

} // namespace core
