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
#include "si4684/Si4684Tuner.hpp"

namespace antenna_calibration {

namespace {

/** Diagnostic-only, 2026-08-23: see net::AntennaCalibration::recalibrateXtal. */
bool recalibrateXtal(std::uint8_t ibias, std::uint8_t ctun,
                     std::uint32_t xtalFreqHz)
{
    return static_cast<bool>(hardware::HardwareBootstrap::si4684Tuner()
                                  .recalibrateXtal(ibias, ctun, xtalFreqHz));
}

} // namespace

net::AntennaCalibration& bridge() noexcept
{
    static net::AntennaCalibration instance{
        .save = &hardware::HardwareBootstrap::saveFmAntCapCalibration,
        .saveDab = &hardware::HardwareBootstrap::saveDabAntCapCalibration,
        .recalibrateXtal = &recalibrateXtal,
    };
    return instance;
}

} // namespace antenna_calibration
