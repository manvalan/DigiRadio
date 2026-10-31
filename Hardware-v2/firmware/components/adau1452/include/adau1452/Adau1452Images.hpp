/**
 * @file    Adau1452Images.hpp
 * @brief   The SigmaStudio+ outputs linked into the app, when present.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

#include "core/IDspProgramSource.hpp"

#include <cstdint>
#include <span>

namespace adau1452 {

/** Self boot image for the 25AA1024; empty when dsp/ does not have it. */
[[nodiscard]] std::span<const std::uint8_t> selfBootImage() noexcept;

/** Small DSP program that lets the host reach the 25AA1024; may be empty. */
[[nodiscard]] std::span<const std::uint8_t> eepromWriterBlob() noexcept;

/**
 * @brief    EmbeddedProgramSource — the RAM download linked into the app.
 *
 * @dname    EmbeddedProgramSource
 * @return   n/a (type)
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-31
 */
class EmbeddedProgramSource final : public core::IDspProgramSource {
public:
    [[nodiscard]] std::expected<core::DspProgram, core::DspProgramError>
    loadProgram() override;
};

} // namespace adau1452
