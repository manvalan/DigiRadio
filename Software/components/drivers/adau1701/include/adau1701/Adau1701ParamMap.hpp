/**
 * @file    Adau1701ParamMap.hpp
 * @brief   SigmaStudio parameter RAM addresses for DigiRadio runtime control.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "DigiRadio_IC_1_PARAM.h"

#include "core/ActiveSource.hpp"
#include "core/EqBandIndex.hpp"

#include <cstdint>

namespace adau1701 {

/**
 * @brief    paramAddrEqBandBase — first coefficient address for a PEQ band.
 *
 * @dname    paramAddrEqBandBase
 * @param    band  Band index 0..5.
 * @return   ADDR_PARAMEQ1_STn_B0 from the SigmaStudio export.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] inline unsigned paramAddrEqBandBase(std::uint8_t band) noexcept
{
    return static_cast<unsigned>(ADDR_PARAMEQ1_ST0_B0)
           + static_cast<unsigned>(band) * 5U;
}

/**
 * @brief    paramSourceIndex — MX1 pair index written to DC1 for a source.
 *
 * @dname    paramSourceIndex
 * @param    source  Which stereo pair MX1 should pass through.
 * @return   0 (Radio), 1 (Bluetooth), or 2 (Beep) -- confirmed live
 *          2026-08-25 against real hardware (see
 *          docs/adau1701-sigmastudio-analysis.md).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-08-25
 */
[[nodiscard]] inline unsigned paramSourceIndex(
    core::ActiveSource source) noexcept
{
    switch (source) {
    case core::ActiveSource::Radio:
        return 0U;
    case core::ActiveSource::Bluetooth:
        return 1U;
    case core::ActiveSource::Beep:
        return 2U;
    }
    return 0U;
}

} // namespace adau1701
