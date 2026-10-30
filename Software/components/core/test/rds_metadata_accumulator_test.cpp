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

    // Each segment is only trusted once it has arrived twice alike.
    for (int pass = 0; pass < 2; ++pass)
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
    if (acc.radiotext()) {
        std::cerr << "radiotext must wait for its end marker\n";
        return EXIT_FAILURE;
    }
    // Segment 1 carries the 0x0D end marker: the text is now complete.
    acc.applyGroup(0x1234U, 0x2001U, 0x0D20U, 0x2020U);

    const auto rt = acc.radiotext();
    if (!rt) {
        std::cerr << "expected radiotext once the end marker arrived\n";
        return EXIT_FAILURE;
    }
    if (rt->value() != "Test") {
        std::cerr << "radiotext mismatch: got '" << rt->value() << "'\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

[[nodiscard]] int runAlternateFrequencyTest()
{
    core::RdsMetadataAccumulator acc;
    // Group 0A (version bit 11 clear): Block C is two AF codes.
    // 105.3 MHz → code 178, 102.7 MHz → code 152.
    const std::uint16_t blockC =
        (static_cast<std::uint16_t>(178U) << 8) | 152U;
    acc.applyGroup(0x1234U, 0x0000U, blockC, 0x5241U);

    const auto af = acc.alternateFrequenciesKhz();
    if (af.size() != 2U) {
        std::cerr << "expected 2 AF codes, got " << af.size() << '\n';
        return EXIT_FAILURE;
    }
    if (af[0] != 105300U || af[1] != 102700U) {
        std::cerr << "AF kHz mismatch\n";
        return EXIT_FAILURE;
    }

    // Group 0B must not treat Block C as AF.
    core::RdsMetadataAccumulator accB;
    accB.applyGroup(0x1234U, 0x0800U, blockC, 0x5241U);
    if (!accB.alternateFrequenciesKhz().empty()) {
        std::cerr << "0B must not ingest AF from Block C\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * PI (block A) is only trusted once two consecutive groups agree, so a
 * single corrupted block A cannot make the AF follower reject (or accept)
 * a transmitter by mistake. reset() forgets it on retune.
 */
[[nodiscard]] int runProgramIdentificationTest()
{
    core::RdsMetadataAccumulator acc;
    acc.applyGroup(0x5201U, 0x0800U, 0x0000U, 0x2020U);
    if (acc.programIdentification()) {
        std::cerr << "PI must not be confirmed after one group\n";
        return EXIT_FAILURE;
    }
    acc.applyGroup(0x5A01U, 0x0800U, 0x0000U, 0x2020U); // corrupted A
    acc.applyGroup(0x5201U, 0x0800U, 0x0000U, 0x2020U);
    if (acc.programIdentification()) {
        std::cerr << "PI must need two consecutive matching groups\n";
        return EXIT_FAILURE;
    }
    acc.applyGroup(0x5201U, 0x0800U, 0x0000U, 0x2020U);
    if (acc.programIdentification() != 0x5201U) {
        std::cerr << "PI 0x5201 expected after two matching groups\n";
        return EXIT_FAILURE;
    }
    acc.reset();
    if (acc.programIdentification()) {
        std::cerr << "reset() must clear PI\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void feedPs(core::RdsMetadataAccumulator& acc, const char* text,
            std::uint8_t blockErrors = 0U)
{
    for (std::uint16_t seg = 0U; seg < 4U; ++seg) {
        const auto d = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(text[seg * 2U]) << 8)
            | static_cast<std::uint8_t>(text[seg * 2U + 1U]));
        acc.applyGroup(0x1234U, static_cast<std::uint16_t>(0x0800U | seg),
                       0x1234U, d, blockErrors);
    }
}

/*
 * A station that scrolls its PS ("RADIO 1 ", then "  NEWS  ") must not
 * come out as a mix of the two, and the old name stays up while the next
 * one is confirmed. An all-blank PS never replaces a real one.
 */
[[nodiscard]] int runScrollingPsTest()
{
    core::RdsMetadataAccumulator acc;
    feedPs(acc, "RADIO 1 ");
    if (acc.programName()) {
        std::cerr << "PS must not be published after one pass\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "RADIO 1 ");
    feedPs(acc, "  NEWS  ");
    if (!acc.programName() || acc.programName()->value() != "RADIO 1") {
        std::cerr << "old PS must stay while the next is assembled\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "  NEWS  ");
    if (!acc.programName() || acc.programName()->value() != "NEWS") {
        std::cerr << "scrolled PS mismatch\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "        ");
    feedPs(acc, "        ");
    if (!acc.programName() || acc.programName()->value() != "NEWS") {
        std::cerr << "blank PS must not replace the name\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * Groups with errors the chip could only correct with 3+ bits flipped
 * (BLE = 2) or not at all (3) carry no text; control characters are
 * never taken as PS.
 */
[[nodiscard]] int runBlockErrorTest()
{
    core::RdsMetadataAccumulator acc;
    feedPs(acc, "BADNAME!", 0x02U); // BLED = 2
    feedPs(acc, "BADNAME!", 0x02U);
    if (acc.programName()) {
        std::cerr << "PS from BLED=2 blocks must be ignored\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "BADNAME!", 0x30U); // BLEB = 3: whole group dropped
    feedPs(acc, "BADNAME!", 0x30U);
    if (acc.programName()) {
        std::cerr << "groups with uncorrectable block B must be dropped\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "AB\x01" "DEFGH");
    feedPs(acc, "AB\x01" "DEFGH");
    if (acc.programName()) {
        std::cerr << "PS with a control code must be ignored\n";
        return EXIT_FAILURE;
    }
    feedPs(acc, "GOODNAME", 0x55U); // all blocks 1-2 bits corrected
    feedPs(acc, "GOODNAME", 0x55U);
    if (!acc.programName() || acc.programName()->value() != "GOODNAME") {
        std::cerr << "PS from BLE=1 blocks must be accepted\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * 2B radiotext: two characters per segment, in block D only (block C
 * repeats the PI). A new text (A/B flip) replaces the old one only once
 * it is complete.
 */
[[nodiscard]] int runRadiotextVersionBTest()
{
    core::RdsMetadataAccumulator acc;
    acc.applyGroup(0x1234U, 0x2800U, 0x1234U, 0x4869U);  // seg 0 "Hi"
    acc.applyGroup(0x1234U, 0x2801U, 0x1234U, 0x0D00U);  // seg 1 end
    if (!acc.radiotext() || acc.radiotext()->value() != "Hi") {
        std::cerr << "2B radiotext mismatch\n";
        return EXIT_FAILURE;
    }
    acc.applyGroup(0x1234U, 0x2810U, 0x1234U, 0x596FU);  // B flag, "Yo"
    if (!acc.radiotext() || acc.radiotext()->value() != "Hi") {
        std::cerr << "old RT must stay until the new one is complete\n";
        return EXIT_FAILURE;
    }
    acc.applyGroup(0x1234U, 0x2811U, 0x1234U, 0x0D00U);
    if (!acc.radiotext() || acc.radiotext()->value() != "Yo") {
        std::cerr << "new RT expected after A/B flip completed\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main()
{
    if (runScrollingPsTest() != EXIT_SUCCESS
        || runBlockErrorTest() != EXIT_SUCCESS
        || runRadiotextVersionBTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runProgramServiceNameTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runIncompleteSegmentsTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runRadiotextTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runAlternateFrequencyTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (runProgramIdentificationTest() != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
