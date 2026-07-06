/**
 * @file    TunerBand.hpp
 * @brief   Strong band selector for tuner operations (core domain).
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
 * @brief    TunerBand — active RF application (DAB or FM image).
 *
 * @dname    TunerBand
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class TunerBand { Dab, Fm };

} // namespace core
