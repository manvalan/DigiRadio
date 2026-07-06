/**
 * @file    EqBandIndex.hpp
 * @brief   Strong index for ADAU1701 Param EQ1 bands (0–5).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/ParseError.hpp"

#include <cstdint>
#include <expected>

namespace core {

/**
 * @brief    EqBandIndex — validated index into the 6-band Param EQ1 module.
 *
 * @dname    EqBandIndex
 * @return   n/a (type)
 * @pubstate Owns index_ in 0..kBandCount-1 after construction.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class EqBandIndex {
public:
    /** Number of parametric EQ bands in the SigmaStudio export. */
    static constexpr std::uint8_t kBandCount = 6U;

    /**
     * @brief    tryFromIndex — validate a band index at the boundary.
     *
     * @dname    tryFromIndex
     * @param    index  Untrusted band index from JSON input.
     * @return   EqBandIndex on success, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<EqBandIndex, ParseError> tryFromIndex(
        std::uint32_t index) noexcept;

    /**
     * @brief    value — read the stored band index.
     *
     * @dname    value
     * @return   Band index 0..5.
     * @pubstate reads index_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

private:
    explicit EqBandIndex(std::uint8_t index) noexcept;

    std::uint8_t index_;
};

} // namespace core
