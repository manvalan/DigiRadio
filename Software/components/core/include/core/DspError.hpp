/**
 * @file    DspError.hpp
 * @brief   Typed errors for DSP control operations.
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
 * @brief    DspError — failure causes for IDsp operations.
 *
 * @dname    DspError
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class DspError {
    NotBooted,
    SafeloadFailed,
    InvalidParameter,
};

} // namespace core
