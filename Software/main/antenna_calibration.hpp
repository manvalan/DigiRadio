/**
 * @file    antenna_calibration.hpp
 * @brief   net::AntennaCalibration implementation over HardwareBootstrap.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "net/SetupWebServer.hpp"

namespace antenna_calibration {

/**
 * @brief    bridge — the process-lifetime AntennaCalibration instance.
 *
 * @dname    bridge
 * @return   Function-pointer table bound to
 *           HardwareBootstrap::saveFmAntCapCalibration, for
 *           net::HttpRouteContext::antennaCalibration /
 *           POST /api/tuner/calibrate-antenna.
 * @pubstate none
 */
[[nodiscard]] net::AntennaCalibration& bridge() noexcept;

} // namespace antenna_calibration
