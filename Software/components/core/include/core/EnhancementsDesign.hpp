/**
 * @file    EnhancementsDesign.hpp
 * @brief   Map enhancement levels onto Param EQ1 bands (host-testable).
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

#include "core/AudioEnhancements.hpp"
#include "core/EqProfile.hpp"

namespace core {

/**
 * @brief    applyEnhancementsToEq — merge enhancement overlays into an EQ profile.
 *
 * @dname    applyEnhancementsToEq
 * @param    base         User EQ settings (bands not touched stay unchanged).
 * @param    enhancements Stereo and bass levels (0 = use base band only).
 * @return   EqProfile with affected PEQ bands updated for safeload.
 * @pubstate none
 *
 * Stereo (bands 3–5): slight 1\,kHz cut plus 3/8\,kHz lift for depth.
 * Bass (bands 1–2): 100\,Hz and 400\,Hz peaking boost.
 * Band 0 (high-pass) is never modified.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] EqProfile applyEnhancementsToEq(
    const EqProfile& base, const AudioEnhancements& enhancements) noexcept;

} // namespace core
