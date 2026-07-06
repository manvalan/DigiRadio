/**
 * @file    Bt1035Error.hpp
 * @brief   Typed errors for FSC-BT1035 driver operations.
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

namespace bt1035 {

/**
 * @brief    Bt1035Error — failure causes for BT1035 bring-up and AT I/O.
 *
 * @dname    Bt1035Error
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class Bt1035Error {
    ResetFailed,
    UartInitFailed,
    NotBooted,
    AtTimeout,
    AtError,
    UnexpectedResponse,
};

} // namespace bt1035
