/**
 * @file    enhancements_design_test.cpp
 * @brief   Host tests for enhancement-to-EQ mapping.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/AudioEnhancements.hpp"
#include "core/EnhanceLevel.hpp"
#include "core/EnhancementsDesign.hpp"
#include "core/EqBandIndex.hpp"
#include "core/EqProfile.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] bool nearlyEqual(float a, float b) noexcept
{
    return std::fabs(a - b) < 0.01F;
}

[[nodiscard]] int runBassMappingTest()
{
    const auto level = core::EnhanceLevel::tryFromLevel(100U);
    if (!level) {
        std::cerr << "level setup failed\n";
        return EXIT_FAILURE;
    }

    core::AudioEnhancements enhancements = core::AudioEnhancements::factoryDefault();
    enhancements.bass = *level;

    const core::EqProfile base = core::EqProfile::factoryDefault();
    const core::EqProfile effective =
        core::applyEnhancementsToEq(base, enhancements);

    const auto band1 = core::EqBandIndex::tryFromIndex(1U);
    const auto band2 = core::EqBandIndex::tryFromIndex(2U);
    if (!band1 || !band2) {
        return EXIT_FAILURE;
    }

    if (!nearlyEqual(effective.band(*band1).gain.value(), 9.0F)
        || !nearlyEqual(effective.band(*band2).gain.value(), 3.0F)) {
        std::cerr << "bass mapping mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runStereoMappingTest()
{
    const auto level = core::EnhanceLevel::tryFromLevel(50U);
    if (!level) {
        std::cerr << "level setup failed\n";
        return EXIT_FAILURE;
    }

    core::AudioEnhancements enhancements = core::AudioEnhancements::factoryDefault();
    enhancements.stereo = *level;

    const core::EqProfile base = core::EqProfile::factoryDefault();
    const core::EqProfile effective =
        core::applyEnhancementsToEq(base, enhancements);

    const auto band3 = core::EqBandIndex::tryFromIndex(3U);
    const auto band4 = core::EqBandIndex::tryFromIndex(4U);
    const auto band5 = core::EqBandIndex::tryFromIndex(5U);
    if (!band3 || !band4 || !band5) {
        return EXIT_FAILURE;
    }

    if (!nearlyEqual(effective.band(*band3).gain.value(), -0.75F)
        || !nearlyEqual(effective.band(*band4).gain.value(), 1.0F)
        || !nearlyEqual(effective.band(*band5).gain.value(), 2.0F)) {
        std::cerr << "stereo mapping mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runOffLeavesBaseTest()
{
    const core::EqProfile base = core::EqProfile::factoryDefault();
    const core::AudioEnhancements off = core::AudioEnhancements::factoryDefault();
    const core::EqProfile effective = core::applyEnhancementsToEq(base, off);

    const auto band1 = core::EqBandIndex::tryFromIndex(1U);
    if (!band1) {
        return EXIT_FAILURE;
    }

    if (!nearlyEqual(effective.band(*band1).gain.value(),
                     base.band(*band1).gain.value())) {
        std::cerr << "off should leave base unchanged\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runBassMappingTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runStereoMappingTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runOffLeavesBaseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
