/**
 * @file    power_monitor.hpp
 * @brief   Battery and charger status over I2C (MAX17048, BQ25896).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include <string>

namespace power_monitor {

/**
 * @brief    start — poll both chips in a low priority task.
 *
 * @dname    start
 * @return   true when the task runs.
 * @pubstate polling is the baseline; the alert line only makes a poll happen
 *           sooner, and clears the fuel gauge's latched alert.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] bool start();

/**
 * @brief    systemJsonWithPower — the "system" object of GET /api/health,
 *           with a "power" member added.
 *
 * @dname    systemJsonWithPower
 * @return   JSON text.
 * @pubstate reads the last polled values.
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
[[nodiscard]] std::string systemJsonWithPower();

} // namespace power_monitor
