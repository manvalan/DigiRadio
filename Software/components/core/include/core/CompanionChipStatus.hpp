/**
 * @file    CompanionChipStatus.hpp
 * @brief   Boot-ready flags for Si4684, ADAU1701, and FSC-BT1035.
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

namespace core {

/**
 * @brief    CompanionChipStatus — companion-chip boot snapshot for /api/health.
 *
 * @dname    CompanionChipStatus
 * @return   n/a (type)
 * @pubstate Immutable aggregate; all true after successful HardwareBootstrap.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct CompanionChipStatus {
    bool si4684Ready;   ///< Si4684 HOST_LOAD completed.
    bool adau1701Ready; ///< ADAU1701 SigmaStudio download completed.
    bool bt1035Ready;   ///< BT1035 I2S init (AUXCFG=3 + I2SCFG=67) completed.
};

} // namespace core
