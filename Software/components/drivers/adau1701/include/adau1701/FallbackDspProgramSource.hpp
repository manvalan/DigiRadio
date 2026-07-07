/**
 * @file    FallbackDspProgramSource.hpp
 * @brief   Tries flash partition first, then embedded SigmaStudio export.
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
 * @brief    FallbackDspProgramSource — flash then embedded program loader.
 *
 * @dname    FallbackDspProgramSource
 * @return   n/a (type)
 * @pubstate Borrows primary and fallback sources for the process lifetime.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class FallbackDspProgramSource : public core::IDspProgramSource {
public:
    /**
     * @brief    FallbackDspProgramSource — wire flash and embedded sources.
     *
     * @dname    FallbackDspProgramSource
     * @param    primary   Usually FlashDspProgramSource.
     * @param    fallback  Usually EmbeddedDspProgramSource.
     * @pubstate stores non-owning references.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    FallbackDspProgramSource(core::IDspProgramSource& primary,
                             core::IDspProgramSource& fallback);

    /**
     * @brief    loadProgram — use flash when valid, else embedded export.
     *
     * @dname    loadProgram
     * @return   DspProgram from primary or fallback.
     * @pubstate logs when falling back to embedded.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<core::DspProgram, core::DspProgramError>
    loadProgram() override;

private:
    core::IDspProgramSource& primary_;
    core::IDspProgramSource& fallback_;
};

} // namespace adau1701
