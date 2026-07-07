/**
 * @file    FlashDspProgramSource.hpp
 * @brief   IDspProgramSource reading the dsp flash partition.
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

#include "core/IDspProgramSource.hpp"

#include <cstdint>
#include <span>

namespace adau1701 {

/**
 * @brief    FlashDspProgramSource — loads a validated blob from partition dsp.
 *
 * @dname    FlashDspProgramSource
 * @return   n/a (type)
 * @pubstate Uses esp_partition API; empty/erased flash returns Empty.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class FlashDspProgramSource : public core::IDspProgramSource {
public:
    /**
     * @brief    loadProgram — read and parse the dsp data partition.
     *
     * @dname    loadProgram
     * @return   DspProgram on success, or a parse/flash error.
     * @pubstate reads the full partition into RAM once per call.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<core::DspProgram, core::DspProgramError>
    loadProgram() override;

    /**
     * @brief    storeBlob — erase and write a validated blob to partition dsp.
     *
     * @dname    storeBlob
     * @param    blob  Framed program bytes (already validated by caller).
     * @return   Ok on success, or DspProgramError::FlashWriteFailed.
     * @pubstate erases the dsp partition before writing.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] static std::expected<void, core::DspProgramError>
    storeBlob(std::span<const std::uint8_t> blob);
};

} // namespace adau1701
