/**
 * @file    EmbeddedDspProgramSource.cpp
 * @brief   EmbeddedDspProgramSource implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "adau1701/EmbeddedDspProgramSource.hpp"

#include "DigiRadio_IC_1.h"
#include "DigiRadio_IC_1_REG.h"

#include <vector>

namespace adau1701 {

namespace {

[[nodiscard]] std::vector<std::uint8_t> copyBytes(const ADI_REG_TYPE* data,
                                                  std::size_t size)
{
    return std::vector<std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(data),
        reinterpret_cast<const std::uint8_t*>(data) + size);
}

} // namespace

std::expected<core::DspProgram, core::DspProgramError>
EmbeddedDspProgramSource::loadProgram()
{
    std::vector<core::RegisterWrite> writes;
    writes.reserve(5U);

    writes.emplace_back(
        static_cast<std::uint16_t>(REG_COREREGISTER_IC_1_ADDR),
        copyBytes(R0_COREREGISTER_IC_1_Default, REG_COREREGISTER_IC_1_BYTE));
    writes.emplace_back(
        static_cast<std::uint16_t>(PROGRAM_ADDR_IC_1),
        copyBytes(Program_Data_IC_1, PROGRAM_SIZE_IC_1));
    writes.emplace_back(
        static_cast<std::uint16_t>(PARAM_ADDR_IC_1),
        copyBytes(Param_Data_IC_1, PARAM_SIZE_IC_1));
    writes.emplace_back(
        static_cast<std::uint16_t>(REG_COREREGISTER_IC_1_ADDR),
        copyBytes(R3_HWCONFIGURATION_IC_1_Default, R3_HWCONFIGURATION_IC_1_SIZE));
    writes.emplace_back(
        static_cast<std::uint16_t>(REG_COREREGISTER_IC_1_ADDR),
        copyBytes(R4_COREREGISTER_IC_1_Default, REG_COREREGISTER_IC_1_BYTE));

    return core::DspProgram(std::move(writes));
}

} // namespace adau1701
