/**
 * @file    Adau1452Images.cpp
 * @brief   Access to the embedded SigmaStudio+ outputs.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "adau1452/Adau1452Images.hpp"

#include "core/DspProgramBlob.hpp"

#define EMBEDDED_SPAN(sym)                                                     \
    []() -> std::span<const std::uint8_t> {                                    \
        extern const std::uint8_t start[] asm("_binary_" sym "_bin_start");    \
        extern const std::uint8_t end[] asm("_binary_" sym "_bin_end");        \
        return {start, end};                                                   \
    }()

namespace adau1452 {

std::span<const std::uint8_t> selfBootImage() noexcept
{
#if ADAU1452_HAS_SELFBOOT
    return EMBEDDED_SPAN("adau1452_selfboot");
#else
    return {};
#endif
}

std::span<const std::uint8_t> eepromWriterBlob() noexcept
{
#if ADAU1452_HAS_EEPROM_WRITER
    return EMBEDDED_SPAN("adau1452_eeprom_writer");
#else
    return {};
#endif
}

std::expected<core::DspProgram, core::DspProgramError>
EmbeddedProgramSource::loadProgram()
{
#if ADAU1452_HAS_PROGRAM
    return core::parseDspProgramBlob(EMBEDDED_SPAN("adau1452_program"));
#else
    return std::unexpected(core::DspProgramError::InvalidMagic);
#endif
}

} // namespace adau1452
