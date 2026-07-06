/**
 * @file    Si4684Band.hpp
 * @brief   Tuner band selection for Si4684 firmware images.
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

namespace si4684 {

/**
 * @brief    Si4684Band — application image loaded after ROM patch.
 *
 * @dname    Si4684Band
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Si4684Band {
    Dab,
    Fm,
};

} // namespace si4684
