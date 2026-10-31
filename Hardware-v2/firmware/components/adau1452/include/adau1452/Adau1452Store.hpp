/**
 * @file    Adau1452Store.hpp
 * @brief   DSP images uploaded over the web, kept in the "dsp" partition.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-01
 */
#pragma once

#include "core/IDspProgramSource.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace adau1452::store {

/**
 * @brief    saveProgram — keep a RAM download, used instead of the EEPROM.
 *
 * @dname    saveProgram
 * @param    blob  DRAD blob, already validated by the caller.
 * @return   true when stored.
 * @pubstate rewrites the program slot; from the next boot SELFBOOT stays low.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] bool saveProgram(std::span<const std::uint8_t> blob);

/**
 * @brief    clearProgram — drop the uploaded program.
 *
 * @dname    clearProgram
 * @return   true when the slot is empty.
 * @pubstate from the next boot the EEPROM runs, or the program in the app.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] bool clearProgram();

/**
 * @brief    hasProgram — whether an uploaded program is stored and intact.
 *
 * @dname    hasProgram
 * @return   true when the program slot checks out.
 * @pubstate reads flash.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] bool hasProgram();

/**
 * @brief    saveEepromImage — keep a self boot image to write at next boot.
 *
 * @dname    saveEepromImage
 * @param    image  Raw EEPROM bytes.
 * @return   true when stored.
 * @pubstate rewrites the image slot, clears any uploaded program and forgets
 *           the last image written, so the next boot writes this one.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] bool saveEepromImage(std::span<const std::uint8_t> image);

/**
 * @brief    loadEepromImage — the uploaded self boot image, if any.
 *
 * @dname    loadEepromImage
 * @return   The bytes, empty when none is stored or the slot is damaged.
 * @pubstate reads flash.
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
[[nodiscard]] std::vector<std::uint8_t> loadEepromImage();

/**
 * @brief    ProgramSource — uploaded program first, then the one in the app.
 *
 * @dname    ProgramSource
 * @return   n/a (type)
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-11-01
 */
class ProgramSource final : public core::IDspProgramSource {
public:
    [[nodiscard]] std::expected<core::DspProgram, core::DspProgramError>
    loadProgram() override;
};

} // namespace adau1452::store
