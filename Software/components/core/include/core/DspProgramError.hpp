/**
 * @file    DspProgramError.hpp
 * @brief   Failure causes for ADAU1701 program blob parse and load.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

namespace core {

/**
 * @brief    DspProgramError — ADAU1701 program validation failures.
 *
 * @dname    DspProgramError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
enum class DspProgramError {
    Empty,       ///< No bytes supplied.
    Truncated,   ///< Header or write payload shorter than declared.
    InvalidMagic,
    UnsupportedVersion,
    BadCrc,
    TooLarge,
    FlashReadFailed,
    FlashWriteFailed,
};

} // namespace core
