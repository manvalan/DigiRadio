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

// Declared at namespace scope with their own names. Block-scope externs called
// the same in several functions are one entity, whose first asm label wins: all
// three functions then returned the program.
#if ADAU1452_HAS_PROGRAM
extern const std::uint8_t kProgramStart[] asm("_binary_adau1452_program_bin_start");
extern const std::uint8_t kProgramEnd[] asm("_binary_adau1452_program_bin_end");
#endif
#if ADAU1452_HAS_SELFBOOT
extern const std::uint8_t kSelfBootStart[] asm("_binary_adau1452_selfboot_bin_start");
extern const std::uint8_t kSelfBootEnd[] asm("_binary_adau1452_selfboot_bin_end");
#endif
#if ADAU1452_HAS_EEPROM_WRITER
extern const std::uint8_t kWriterStart[] asm("_binary_adau1452_eeprom_writer_bin_start");
extern const std::uint8_t kWriterEnd[] asm("_binary_adau1452_eeprom_writer_bin_end");
#endif

namespace adau1452 {

std::span<const std::uint8_t> selfBootImage() noexcept
{
#if ADAU1452_HAS_SELFBOOT
    return {kSelfBootStart, kSelfBootEnd};
#else
    return {};
#endif
}

std::span<const std::uint8_t> eepromWriterBlob() noexcept
{
#if ADAU1452_HAS_EEPROM_WRITER
    return {kWriterStart, kWriterEnd};
#else
    return {};
#endif
}

std::expected<core::DspProgram, core::DspProgramError>
EmbeddedProgramSource::loadProgram()
{
#if ADAU1452_HAS_PROGRAM
    return core::parseDspProgramBlob(std::span<const std::uint8_t>(kProgramStart, kProgramEnd));
#else
    return std::unexpected(core::DspProgramError::InvalidMagic);
#endif
}

} // namespace adau1452
