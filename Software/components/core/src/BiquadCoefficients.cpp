/**
 * @file    BiquadCoefficients.cpp
 * @brief   BiquadCoefficients fixpoint conversion.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BiquadCoefficients.hpp"
#include "core/BiquadDesign.hpp"

namespace core {

std::array<std::int32_t, 5> BiquadCoefficients::toFixpoint823() const noexcept
{
    return {
        floatToFixpoint823(b0),
        floatToFixpoint823(b1),
        floatToFixpoint823(b2),
        floatToFixpoint823(a0),
        floatToFixpoint823(a1),
    };
}

} // namespace core
