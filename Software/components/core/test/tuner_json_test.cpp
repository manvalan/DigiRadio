/**
 * @file    tuner_json_test.cpp
 * @brief   Host tests for tuner JSON parse/serialise.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/FrequencyKHz.hpp"
#include "core/ParseError.hpp"
#include "core/TunerBand.hpp"
#include "core/TunerJson.hpp"
#include "core/TunerStatus.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] bool expectEqual(const std::string& actual,
                               const std::string& expected)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << "expected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

[[nodiscard]] int runTunerTuneParseDabTest()
{
    const auto parsed =
        core::parseTunerTuneJson(R"({"band":"dab","freq_index":12})");
    if (!parsed || parsed->band != core::TunerBand::Dab
        || parsed->dabFreqIndex != 12U) {
        std::cerr << "DAB tune parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerTuneParseFmTest()
{
    const auto parsed =
        core::parseTunerTuneJson(R"({"band":"fm","frequency_khz":101500})");
    if (!parsed || parsed->band != core::TunerBand::Fm
        || !parsed->fmFrequency
        || parsed->fmFrequency->value() != 101500U) {
        std::cerr << "FM tune parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerTuneRejectTest()
{
    const auto parsed =
        core::parseTunerTuneJson(R"({"band":"fm","frequency_khz":50000})");
    if (parsed) {
        std::cerr << "expected FM frequency rejection\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerPlayParseTest()
{
    const auto parsed =
        core::parseTunerPlayJson(R"({"service_id":42,"component_id":7})");
    if (!parsed || parsed->serviceId != 42U || parsed->componentId != 7U) {
        std::cerr << "play parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerStatusSerialiseTest()
{
    core::TunerStatus status = {};
    status.booted = true;
    status.band = core::TunerBand::Fm;
    status.locked = true;
    status.volume = 40;
    status.fmFrequency = *core::FrequencyKHz::tryFromKhz(101500U);
    status.fmRssiDbuV = -20;
    status.fmSnrDb = 25;
    status.fmStereo = true;

    const std::string json = core::serializeTunerStatusJson(status);
    if (json.find("\"booted\":true") == std::string::npos
        || json.find("\"band\":\"fm\"") == std::string::npos
        || json.find("\"frequency_khz\":101500") == std::string::npos) {
        std::cerr << "status serialise missing fields: " << json << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerServicesSerialiseTest()
{
    core::TunerServiceEntry entry = {};
    entry.serviceId = 1U;
    entry.componentId = 2U;
    entry.label = {'R', 'A', 'I', '\0'};

    const std::string json =
        core::serializeTunerServicesJson({entry});
    if (!expectEqual(json,
                     R"({"services":[{"service_id":1,"component_id":2,"label":"RAI"}]})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runTunerErrorSerialiseTest()
{
    if (!expectEqual(core::serializeTunerErrorJson("not_booted"),
                     R"({"status":"error","reason":"not_booted"})")) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runTunerTuneParseDabTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerTuneParseFmTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerTuneRejectTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerPlayParseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerStatusSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerServicesSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runTunerErrorSerialiseTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
