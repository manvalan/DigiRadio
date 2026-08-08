/**
 * @file    BtSpeakerTarget.hpp
 * @brief   Saved A2DP speaker target (MAC + optional friendly name).
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

#include <string>

namespace core {

/**
 * @brief    BtSpeakerTarget — default BR/EDR speaker for AT+A2DPCONN.
 *
 * @dname    BtSpeakerTarget
 * @return   n/a (type)
 * @pubstate Persisted in NVS via ISecureStore; MAC is 12-char ASCII hex.
 *
 * @author   Michele Bigi
 * @date     2026-08-05
 */
struct BtSpeakerTarget {
    std::string mac;  ///< 12-char BR/EDR MAC (uppercase, no separators).
    std::string name; ///< Optional label shown in the web UI.
};

} // namespace core
