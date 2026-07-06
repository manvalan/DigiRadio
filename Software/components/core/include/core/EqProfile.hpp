/**
 * @file    EqProfile.hpp
 * @brief   Six-band parametric EQ profile for the ADAU1701.
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

#include "core/EqBandIndex.hpp"
#include "core/EqBandSettings.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

#include <array>

namespace core {

/**
 * @brief    EqProfile — six PEQ bands persisted and applied via AudioService.
 *
 * @dname    EqProfile
 * @return   n/a (type)
 * @pubstate Owns bands_ array; default centres match typical listening curve.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class EqProfile {
public:
    /**
     * @brief    factoryDefault — flat EQ at standard centre frequencies.
     *
     * @dname    factoryDefault
     * @return   EqProfile with 0 dB on all bands.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static EqProfile factoryDefault() noexcept;

    /**
     * @brief    band — read settings for one PEQ band.
     *
     * @dname    band
     * @param    index  Band index 0..5.
     * @return   Const reference to band settings.
     * @pubstate reads bands_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const EqBandSettings& band(EqBandIndex index) const noexcept;

    /**
     * @brief    setBand — replace settings for one PEQ band.
     *
     * @dname    setBand
     * @param    index     Band index 0..5.
     * @param    settings  New gain, centre, and Q.
     * @pubstate writes bands_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void setBand(EqBandIndex index, EqBandSettings settings) noexcept;

    /**
     * @brief    bands — access the full band array.
     *
     * @dname    bands
     * @return   Const reference to all six band settings.
     * @pubstate reads bands_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const std::array<EqBandSettings, EqBandIndex::kBandCount>&
    bands() const noexcept;

private:
    explicit EqProfile(std::array<EqBandSettings, EqBandIndex::kBandCount> bands)
        noexcept;

    std::array<EqBandSettings, EqBandIndex::kBandCount> bands_;
};

} // namespace core
