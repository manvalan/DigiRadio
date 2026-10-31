/**
 * @file    sigma_bridge.hpp
 * @brief   SigmaStudio+ TCP/IP channel to the ADAU1452, port 8086.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-01
 */
#pragma once

#include "adau1452/Adau1452Driver.hpp"

namespace sigma_bridge {

/**
 * @brief    start — serve SigmaStudio+ over the network.
 *
 * @dname    start
 * @param    dsp  Driver with static lifetime.
 * @return   true when the task runs.
 * @pubstate one client at a time. Same trust as v1's bridge: anyone on the
 *           LAN can reach the DSP. Lets SigmaStudio+ download, tune and write
 *           the self boot EEPROM through the DSP with no USBi attached.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] bool start(adau1452::Adau1452Driver& dsp);

} // namespace sigma_bridge
