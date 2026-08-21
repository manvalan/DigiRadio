/**
 * @file    antenna_calibration.cpp
 * @brief   net::AntennaCalibration implementation over HardwareBootstrap.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "antenna_calibration.hpp"

#include "hardware_bootstrap.hpp"

namespace antenna_calibration {

net::AntennaCalibration& bridge() noexcept
{
    static net::AntennaCalibration instance{
        .save = &hardware::HardwareBootstrap::saveFmAntCapCalibration,
        .saveDab = &hardware::HardwareBootstrap::saveDabAntCapCalibration,
    };
    return instance;
}

} // namespace antenna_calibration
