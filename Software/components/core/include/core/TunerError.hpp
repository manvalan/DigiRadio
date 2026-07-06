/**
 * @file    TunerError.hpp
 * @brief   Typed errors for tuner service and ITuner adapters.
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
 * @brief    TunerError — failure causes for tuner operations.
 *
 * @dname    TunerError
 * @return   n/a (type)
 * @pubstate Stable error set mapped from driver failures at the service boundary.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class TunerError {
    NotBooted,
    WrongBand,
    HardwareFailed,
    TuneFailed,
    ServiceListEmpty,
    InvalidInput,
};

} // namespace core
