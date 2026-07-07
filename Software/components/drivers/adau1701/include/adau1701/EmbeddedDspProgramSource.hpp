/**
 * @file    EmbeddedDspProgramSource.hpp
 * @brief   IDspProgramSource backed by the compiled SigmaStudio export.
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

namespace adau1701 {

/**
 * @brief    EmbeddedDspProgramSource — factory default from DigiRadio_IC_1.h.
 *
 * @dname    EmbeddedDspProgramSource
 * @return   n/a (type)
 * @pubstate Builds the same five SIGMA_WRITE steps as default_download_IC_1().
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class EmbeddedDspProgramSource : public core::IDspProgramSource {
public:
    /**
     * @brief    loadProgram — return the embedded SigmaStudio download script.
     *
     * @dname    loadProgram
     * @return   DspProgram mirroring default_download_IC_1().
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<core::DspProgram, core::DspProgramError>
    loadProgram() override;
};

} // namespace adau1701
