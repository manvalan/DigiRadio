/**
 * @file    broadcast_metadata_test.cpp
 * @brief   Host tests for broadcast metadata parsing (RDS / DLS).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/BroadcastLabel.hpp"
#include "core/DabDynamicLabelAccumulator.hpp"
#include "core/RdsMetadataAccumulator.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

[[nodiscard]] int runChipLabelTrimTest()
{
    const auto label = core::BroadcastLabel::tryFromChipBytes("  RAI 1 \0\0");
    if (!label || label->value() != "RAI 1") {
        std::cerr << "chip label trim failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runRdsProgramNameTest()
{
    // Group type 0, segment address in blockB bits[1:0] (no shift); PS
    // characters are in blockD, never blockC (ETSI EN 62106 §3.1.5).
    core::RdsMetadataAccumulator acc;
    // Each PS segment is trusted once it has arrived twice alike.
    for (int pass = 0; pass < 2; ++pass) {
        acc.applyGroup(0U, 0x0000U, 0U, 0x5445U);
        acc.applyGroup(0U, 0x0001U, 0U, 0x5354U);
        acc.applyGroup(0U, 0x0002U, 0U, 0x2020U);
        acc.applyGroup(0U, 0x0003U, 0U, 0x2020U);
    }

    const auto name = acc.programName();
    if (!name || name->value() != "TEST") {
        std::cerr << "RDS PS parse failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runRdsRadiotextTest()
{
    core::RdsMetadataAccumulator acc;
    acc.applyGroup(0U, 0x2000U, 0x4E6FU, 0x7720U);
    acc.applyGroup(0U, 0x2001U, 0x0D00U, 0x0000U); // end marker

    const auto text = acc.radiotext();
    if (!text || text->value() != "Now") {
        std::cerr << "RDS RT parse failed: "
                  << (text ? std::string(text->value()) : "nullopt") << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runDabDynamicLabelTest()
{
    core::DabDynamicLabelAccumulator acc;
    const std::vector<std::uint8_t> payload = {'L', 'i', 'v', 'e', ' ', 'D', 'J'};
    acc.applySegment(0U, 1U, payload);

    const auto bytes = acc.payload();
    if (!bytes || !std::equal(bytes->begin(), bytes->end(), payload.begin(),
                              payload.end())) {
        std::cerr << "DLS single-segment join failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runDabDynamicLabelSegmentsTest()
{
    core::DabDynamicLabelAccumulator acc;
    const std::vector<std::uint8_t> part1 = {'A', 'B', 'C'};
    const std::vector<std::uint8_t> part2 = {'D', 'E', 'F'};
    acc.applySegment(0U, 2U, part1);
    if (acc.payload()) {
        std::cerr << "DLS payload reported before the last segment\n";
        return EXIT_FAILURE;
    }
    acc.applySegment(1U, 2U, part2);

    const std::vector<std::uint8_t> joined = {'A', 'B', 'C', 'D', 'E', 'F'};
    const auto bytes = acc.payload();
    if (!bytes || !std::equal(bytes->begin(), bytes->end(), joined.begin(),
                              joined.end())) {
        std::cerr << "DLS multi-segment join failed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runChipLabelTrimTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runRdsProgramNameTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runRdsRadiotextTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runDabDynamicLabelTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runDabDynamicLabelSegmentsTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
