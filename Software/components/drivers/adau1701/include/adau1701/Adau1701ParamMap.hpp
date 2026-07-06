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

#include "core/EqBandIndex.hpp"
#include "core/MixSource.hpp"

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
 * @brief    paramAddrInputLeft — left volume address for an input source.
 *
 * @dname    paramAddrInputLeft
 * @param    source  Si4684 or ESP32 path.
 * @return   Parameter RAM address for the left channel gain cell.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] inline unsigned paramAddrInputLeft(core::MixSource source) noexcept
{
    return source == core::MixSource::Si4684
               ? static_cast<unsigned>(ADDR_SI4674)
               : static_cast<unsigned>(ADDR_ESP32);
}

/**
 * @brief    paramAddrInputRight — right volume address for an input source.
 *
 * @dname    paramAddrInputRight
 * @param    source  Si4684 or ESP32 path.
 * @return   Parameter RAM address for the right channel gain cell.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] inline unsigned paramAddrInputRight(core::MixSource source) noexcept
{
    return source == core::MixSource::Si4684
               ? static_cast<unsigned>(ADDR_SI4674_1)
               : static_cast<unsigned>(ADDR_ESP32_1);
}

} // namespace adau1701
