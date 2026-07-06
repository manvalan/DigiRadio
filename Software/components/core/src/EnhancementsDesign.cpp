/**
 * @file    EnhancementsDesign.cpp
 * @brief   Enhancement-to-EQ mapping implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#include "core/EnhancementsDesign.hpp"

#include "core/EqBandIndex.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

namespace core {

namespace {

void setBand(EqProfile& profile, std::uint8_t index, GainDb gain,
             FrequencyHz center, float q) noexcept
{
    const auto band = EqBandIndex::tryFromIndex(index);
    if (!band) {
        return;
    }
    profile.setBand(*band, EqBandSettings{
                               .gain = gain,
                               .center = center,
                               .q = q,
                           });
}

[[nodiscard]] GainDb gainFromDb(float db) noexcept
{
    return *GainDb::tryFromDb(db);
}

} // namespace

EqProfile applyEnhancementsToEq(const EqProfile& base,
                                const AudioEnhancements& enhancements) noexcept
{
    EqProfile profile = base;

    if (enhancements.bass.value() > 0U) {
        const float t = enhancements.bass.fraction();
        setBand(profile, 1U, gainFromDb(9.0F * t),
                *FrequencyHz::tryFromHz(100U), 0.9F);
        setBand(profile, 2U, gainFromDb(3.0F * t),
                *FrequencyHz::tryFromHz(400U), 1.0F);
    }

    if (enhancements.stereo.value() > 0U) {
        const float t = enhancements.stereo.fraction();
        setBand(profile, 3U, gainFromDb(-1.5F * t),
                *FrequencyHz::tryFromHz(1000U), 1.0F);
        setBand(profile, 4U, gainFromDb(2.0F * t),
                *FrequencyHz::tryFromHz(3000U), 1.0F);
        setBand(profile, 5U, gainFromDb(4.0F * t),
                *FrequencyHz::tryFromHz(8000U), 1.0F);
    }

    return profile;
}

} // namespace core
