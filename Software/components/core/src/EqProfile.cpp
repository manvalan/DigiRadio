/**
 * @file    EqProfile.cpp
 * @brief   EqProfile implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/EqProfile.hpp"

namespace core {

namespace {

constexpr std::array<std::uint32_t, EqBandIndex::kBandCount> kDefaultCenters{
    40U, 100U, 250U, 1000U, 4000U, 12000U};

[[nodiscard]] std::array<EqBandSettings, EqBandIndex::kBandCount>
makeDefaultBands() noexcept
{
    return std::array<EqBandSettings, EqBandIndex::kBandCount>{
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[0]),
                       1.414F},
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[1]),
                       1.414F},
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[2]),
                       1.414F},
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[3]),
                       1.414F},
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[4]),
                       1.414F},
        EqBandSettings{GainDb::zero(), *FrequencyHz::tryFromHz(kDefaultCenters[5]),
                       1.414F},
    };
}

} // namespace

EqProfile::EqProfile(
    std::array<EqBandSettings, EqBandIndex::kBandCount> bands) noexcept
    : bands_(bands)
{
}

EqProfile EqProfile::factoryDefault() noexcept
{
    return EqProfile(makeDefaultBands());
}

const EqBandSettings& EqProfile::band(EqBandIndex index) const noexcept
{
    return bands_[index.value()];
}

void EqProfile::setBand(EqBandIndex index, EqBandSettings settings) noexcept
{
    bands_[index.value()] = settings;
}

const std::array<EqBandSettings, EqBandIndex::kBandCount>& EqProfile::bands()
    const noexcept
{
    return bands_;
}

} // namespace core
