/**
 * @file    DspProgram.cpp
 * @brief   DspProgram implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/DspProgram.hpp"

namespace core {

DspProgram::DspProgram(std::vector<RegisterWrite> writes)
    : writes_(std::move(writes))
{
}

const std::vector<RegisterWrite>& DspProgram::writes() const noexcept
{
    return writes_;
}

} // namespace core
