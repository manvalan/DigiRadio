/**
 * @file    MixSource.hpp
 * @brief   ADAU1701 input path selector for per-source volume control.
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
 * @brief    MixSource — Si4684 tuner or ESP32 I2S input on the ADAU1701.
 *
 * @dname    MixSource
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class MixSource {
    Si4684,
    Esp32,
};

} // namespace core
