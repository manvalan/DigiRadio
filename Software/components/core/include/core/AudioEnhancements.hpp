/**
 * @file    AudioEnhancements.hpp
 * @brief   Stereo width and bass boost enhancement levels.
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

#include "core/EnhanceLevel.hpp"

namespace core {

/**
 * @brief    AudioEnhancements — psychoacoustic EQ overlays (0–100 each).
 *
 * @dname    AudioEnhancements
 * @return   n/a (type)
 * @pubstate Mapped to PEQ bands at runtime (manual chapter sigmastudio).
 *           No dedicated SigmaStudio blocks; see \c applyEnhancementsToEq.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct AudioEnhancements {
    EnhanceLevel stereo; ///< Stereo depth / presence (PEQ bands 3–5).
    EnhanceLevel bass;   ///< Bass emphasis (PEQ bands 1–2).

    /**
     * @brief    factoryDefault — both enhancements off.
     *
     * @dname    factoryDefault
     * @return   AudioEnhancements at level 0.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static AudioEnhancements factoryDefault() noexcept;
};

} // namespace core
