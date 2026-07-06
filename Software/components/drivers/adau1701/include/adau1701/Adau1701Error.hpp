/**
 * @file    Adau1701Error.hpp
 * @brief   Typed errors for ADAU1701 driver operations.
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

namespace adau1701 {

/**
 * @brief    Adau1701Error — failure causes for ADAU1701 bring-up.
 *
 * @dname    Adau1701Error
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Adau1701Error {
    I2cInitFailed,
    ResetFailed,
    DownloadFailed,
    NotBooted,
    SafeloadFailed,
    InvalidParameter,
};

} // namespace adau1701
