/**
 * @file    audio_profile_json_test.cpp
 * @brief   Host tests for AudioProfile JSON round-trip.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/AudioProfile.hpp"
#include "core/AudioProfileJson.hpp"
#include "core/EnhanceLevel.hpp"
#include "core/EqBandIndex.hpp"
#include "core/FrequencyHz.hpp"
#include "core/GainDb.hpp"

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] int runRoundTripTest()
{
    core::AudioProfile profile = core::AudioProfile::factoryDefault();
    const auto band = core::EqBandIndex::tryFromIndex(2U);
    const auto gain = core::GainDb::tryFromDb(3.0F);
    const auto center = core::FrequencyHz::tryFromHz(500U);
    if (!band || !gain || !center) {
        std::cerr << "test setup failed\n";
        return EXIT_FAILURE;
    }
    profile.eq.setBand(*band,
                       core::EqBandSettings{
                           .gain = *gain,
                           .center = *center,
                           .q = 2.0F,
                       });

    const std::string json = core::serializeAudioProfileJson(profile);
    const auto parsed = core::parseAudioProfileJson(json);
    if (!parsed) {
        std::cerr << "parse failed\n";
        return EXIT_FAILURE;
    }

    const auto& b = parsed->eq.band(*band);
    if (b.gain.value() != 3.0F || b.center.value() != 500U
        || b.q != 2.0F) {
        std::cerr << "round-trip mismatch\n";
        return EXIT_FAILURE;
    }

    if (parsed->enhancements.stereo.value() != 0U
        || parsed->enhancements.bass.value() != 0U) {
        std::cerr << "default enhancements mismatch\n";
        return EXIT_FAILURE;
    }

    profile.enhancements.stereo = *core::EnhanceLevel::tryFromLevel(40U);
    profile.enhancements.bass = *core::EnhanceLevel::tryFromLevel(75U);
    const std::string json2 = core::serializeAudioProfileJson(profile);
    const auto parsed2 = core::parseAudioProfileJson(json2);
    if (!parsed2 || parsed2->enhancements.stereo.value() != 40U
        || parsed2->enhancements.bass.value() != 75U) {
        std::cerr << "enhancements round-trip mismatch\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
