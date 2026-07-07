/**
 * @file    IDspProgramSource.hpp
 * @brief   Port for loading an ADAU1701 RAM download program.
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

#include "core/DspProgram.hpp"
#include "core/DspProgramError.hpp"

#include <expected>

namespace core {

/**
 * @brief    IDspProgramSource — supplies DspProgram for Adau1701Driver::boot().
 *
 * @dname    IDspProgramSource
 * @return   n/a (type)
 * @pubstate Implementations live in the adau1701 driver (embedded + flash).
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class IDspProgramSource {
public:
    virtual ~IDspProgramSource() = default;

    /**
     * @brief    loadProgram — obtain the download script for this boot.
     *
     * @dname    loadProgram
     * @return   DspProgram on success, or DspProgramError.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] virtual std::expected<DspProgram, DspProgramError>
    loadProgram() = 0;
};

} // namespace core
