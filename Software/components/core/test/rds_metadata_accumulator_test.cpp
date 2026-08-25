/**
 * @file    rds_metadata_accumulator_test.cpp
 * @brief   Host tests for RdsMetadataAccumulator against ETSI EN 62106
 *          group 0B/2A block layouts.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-08-25
 */

#include "core/RdsMetadataAccumulator.hpp"

#include <cstdlib>
#include <iostream>

namespace {

/*
 * Group type 0B (block B bit 11 set), segment address in bits[1:0]: the two
 * Program Service characters for that segment live in Block D (high byte
 * first), never Block C -- Block C for group 0 carries alternate-frequency
 * codes (0A) or a repeated PI code (0B), not text. This is the exact bug
 * fixed 2026-08-25 (characters were being read from Block C, which made
 * every FM station name silently fail to accumulate).
 */
[[nodiscard]] int runProgramServiceNameTest()
{
    core::RdsMetadataAccumulator acc;

    // "RADIO101" across the 4 PS segments, group type 0B.
    struct
    {
        std::uint16_t segment;
        char c0;
        char c1;
    } segments[] = {
        {0U, 'R', 'A'},
        {1U, 'D', 'I'},
        {2U, 'O', '1'},
        {3U, '0', '1'},
    };

    for (const auto &seg : segments) {
        const std::uint16_t blockB =
            static_cast<std::uint16_t>(0x0800U | seg.segment);
        const std::uint16_t blockD = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(seg.c0) << 8) |
            static_cast<std::uint16_t>(seg.c1));
        // Block C deliberately holds garbage (a repeated-PI-like value that
        // is NOT the expected text) to prove the decoder ignores it for
        // group 0, rather than happening to read the right bytes by luck.
        acc.applyGroup(0x1234U, blockB, 0xBEEFU, blockD);
    }

    const auto name = acc.programName();
    if (!name) {
        std::cerr << "expected a program name after 4 PS segments\n";
        return EXIT_FAILURE;
    }
    if (name->value() != "RADIO101") {
        std::cerr << "program name mismatch: got '" << name->value()
                   << "'\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runIncompleteSegmentsTest()
{
    core::RdsMetadataAccumulator acc;
    acc.applyGroup(0x1234U, 0x0800U, 0xBEEFU, 0x5241U); // segment 0 only
    if (acc.programName()) {
        std::cerr << "program name should be absent with 3 segments "
                     "missing\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * Group type 2A: RadioText characters split across Block C (2 chars) and
 * Block D (2 chars) for the same segment -- unlike group 0, Block C really
 * does carry text here, so this is a regression guard against ever
 * "fixing" group 2 the same way group 0 needed fixing.
 */
[[nodiscard]] int runRadiotextTest()
{
    core::RdsMetadataAccumulator acc;
    const std::uint16_t blockB = 0x2000U; // group type 2, version A, seg 0
    const std::uint16_t blockC =
        (static_cast<std::uint16_t>('T') << 8) | static_cast<std::uint16_t>('e');
    const std::uint16_t blockD =
        (static_cast<std::uint16_t>('s') << 8) | static_cast<std::uint16_t>('t');
    acc.applyGroup(0x1234U, blockB, blockC, blockD);

    const auto rt = acc.radiotext();
    if (!rt) {
        std::cerr << "expected radiotext after one 2A segment\n";
        return EXIT_FAILURE;
    }
    if (rt->value() != "Test") {
        std::cerr << "radiotext mismatch: got '" << rt->value() << "'\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runProgramServiceNameTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runIncompleteSegmentsTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runRadiotextTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
