/**
 * @file    DspProgram.hpp
 * @brief   Ordered ADAU1701 RAM download sequence (pure domain).
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

#include "core/RegisterWrite.hpp"

#include <vector>

namespace core {

/**
 * @brief    DspProgram — immutable SigmaStudio download script.
 *
 * @dname    DspProgram
 * @return   n/a (type)
 * @pubstate Owns writes_; built from a validated flash blob or embedded export.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class DspProgram {
public:
    /**
     * @brief    DspProgram — construct from an ordered write list.
     *
     * @dname    DspProgram
     * @param    writes  Non-empty register blocks in replay order.
     * @pubstate moves writes into writes_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    explicit DspProgram(std::vector<RegisterWrite> writes);

    /**
     * @brief    writes — read the ordered download steps.
     *
     * @dname    writes
     * @return   Reference to the internal write vector.
     * @pubstate reads writes_.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] const std::vector<RegisterWrite>& writes() const noexcept;

private:
    std::vector<RegisterWrite> writes_;
};

} // namespace core
