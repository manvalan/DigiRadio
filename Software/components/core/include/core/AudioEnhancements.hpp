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
 * @brief    AudioEnhancements — dedicated-block enhancement levels (0–100).
 *
 * @dname    AudioEnhancements
 * @return   n/a (type)
 * @pubstate Applied via IDsp::setStereoSpreadLevel/setBassBoostLevel, which
 *           scale the SPhat1/Bass Boost1 SigmaStudio blocks toward their
 *           compiled response; does not touch EqProfile.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct AudioEnhancements {
    EnhanceLevel stereo; ///< Stereo spread (SPhat1 spatializer).
    EnhanceLevel bass;   ///< Bass boost intensity (Bass Boost1).

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
