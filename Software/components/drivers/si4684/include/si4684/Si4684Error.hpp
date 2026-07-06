/**
 * @file    Si4684Error.hpp
 * @brief   Typed errors for Si4684 driver operations.
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
 * @brief    Si4684Error — failure causes for Si4684 bring-up and tuning.
 *
 * @dname    Si4684Error
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Si4684Error {
    SpiInitFailed,
    ResetFailed,
    CtsTimeout,
    PowerUpFailed,
    PatchLoadFailed,
    ImageLoadFailed,
    BootFailed,
    NotBooted,
    WrongBand,
    CommandFailed,
    StcTimeout,
    TuneFailed,
    ReplyTooShort,
    BufferTooSmall,
};

} // namespace si4684
