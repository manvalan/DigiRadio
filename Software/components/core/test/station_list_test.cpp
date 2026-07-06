/**
 * @file    station_list_test.cpp
 * @brief   Host tests for station preset list domain and JSON.
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
#include "core/PresetSlot.hpp"
#include "core/Station.hpp"
#include "core/StationList.hpp"
#include "core/StationListError.hpp"
#include "core/StationListJson.hpp"
#include "core/StationName.hpp"
#include "core/TunerBand.hpp"

#include <cstdlib>
#include <iostream>

namespace {

[[nodiscard]] core::Station makeFmStation(const char* name, std::uint32_t khz,
                                          unsigned slot)
{
    auto label = core::StationName::tryFrom(name);
    auto frequency = core::FrequencyKHz::tryFromKhz(khz);
    auto preset = core::PresetSlot::tryFrom(slot);
    return core::Station(*label, core::TunerBand::Fm, 0U, std::nullopt,
                         std::nullopt, *frequency, *preset);
}

[[nodiscard]] int runDuplicateRejectionTest()
{
    core::StationList list;
    if (auto added = list.add(makeFmStation("Radio 2", 88500U, 1U)); !added) {
        std::cerr << "first add failed\n";
        return EXIT_FAILURE;
    }
    if (list.add(makeFmStation("Radio 2 dup", 88500U, 2U))) {
        std::cerr << "expected duplicate tune target rejection\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runJsonRoundTripTest()
{
    core::StationList list;
    if (auto added = list.add(makeFmStation("Jazz FM", 101500U, 3U)); !added) {
        std::cerr << "add failed\n";
        return EXIT_FAILURE;
    }

    const std::string json = core::serializeStationListJson(list);
    auto parsed = core::parseStationListJson(json);
    if (!parsed || parsed->stations().size() != 1U) {
        std::cerr << "round-trip parse failed\n";
        return EXIT_FAILURE;
    }
    if (parsed->stations()[0U].name().value() != "Jazz FM") {
        std::cerr << "name mismatch after round-trip\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseStationJsonTest()
{
    const auto station = core::parseStationJson(
        R"({"name":"BBC","band":"dab","dab_freq_index":12,"dab_service_id":42,"dab_component_id":1})");
    if (!station || station->band() != core::TunerBand::Dab
        || station->dabFreqIndex() != 12U) {
        std::cerr << "dab station parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runParseStationReorderJsonTest()
{
    const auto parsed =
        core::parseStationReorderJson(R"({"from":1,"to":0})");
    if (!parsed || parsed->fromIndex != 1U || parsed->toIndex != 0U) {
        std::cerr << "reorder parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runStationListReorderTest()
{
    core::StationList list;
    if (auto a = list.add(makeFmStation("A", 88000U, 1U)); !a) {
        return EXIT_FAILURE;
    }
    if (auto b = list.add(makeFmStation("B", 90000U, 2U)); !b) {
        return EXIT_FAILURE;
    }
    if (auto moved = list.move(1U, 0U); !moved) {
        std::cerr << "move failed\n";
        return EXIT_FAILURE;
    }
    if (list.stations()[0U].name().value() != "B") {
        std::cerr << "expected B first after reorder\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runDuplicateRejectionTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runJsonRoundTripTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseStationJsonTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runParseStationReorderJsonTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runStationListReorderTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
