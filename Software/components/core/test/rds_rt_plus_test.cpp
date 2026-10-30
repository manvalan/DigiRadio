/**
 * @file    rds_rt_plus_test.cpp
 * @brief   Host tests for RDS RadioText Plus and EBU Latin output.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/RdsMetadataAccumulator.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(bool ok, std::string_view what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

constexpr std::uint16_t kPi = 0x5264U;
constexpr std::uint16_t kRtPlusGroup = (11U << 1U); // 11A

std::uint16_t chars(char a, char b)
{
    return static_cast<std::uint16_t>((static_cast<std::uint8_t>(a) << 8U)
                                      | static_cast<std::uint8_t>(b));
}

// Group 2A segments for text (padded with 0x0D end marker).
void sendRadiotext(core::RdsMetadataAccumulator& rds, std::string text,
                   bool ab)
{
    text.push_back('\r');
    while (text.size() % 4U != 0U) {
        text.push_back(' ');
    }
    for (std::size_t seg = 0U; seg * 4U < text.size(); ++seg) {
        const auto b = static_cast<std::uint16_t>((2U << 12U)
                                                  | (ab ? 0x10U : 0U) | seg);
        rds.applyGroup(kPi, b, chars(text[seg * 4U], text[seg * 4U + 1U]),
                       chars(text[seg * 4U + 2U], text[seg * 4U + 3U]));
    }
}

void announceRtPlus(core::RdsMetadataAccumulator& rds)
{
    rds.applyGroup(kPi, static_cast<std::uint16_t>((3U << 12U) | kRtPlusGroup),
                   0x0000U, 0x4BD7U);
}

// RT+ group: running, tag1 (type, start, len-1 6 bits), tag2 (5-bit len).
void sendRtPlus(core::RdsMetadataAccumulator& rds, bool running,
                std::uint8_t t1, std::uint8_t s1, std::uint8_t l1,
                std::uint8_t t2, std::uint8_t s2, std::uint8_t l2)
{
    const auto b = static_cast<std::uint16_t>((11U << 12U)
                                              | (running ? 0x08U : 0U)
                                              | (t1 >> 3U));
    const auto c = static_cast<std::uint16_t>(((t1 & 0x07U) << 13U)
                                              | (s1 << 7U) | ((l1 - 1U) << 1U)
                                              | (t2 >> 5U));
    const auto d = static_cast<std::uint16_t>(((t2 & 0x1FU) << 11U)
                                              | (s2 << 5U) | (l2 - 1U));
    rds.applyGroup(kPi, b, c, d);
}

void rtPlusSplitsTitleAndArtist()
{
    core::RdsMetadataAccumulator rds;
    announceRtPlus(rds);
    sendRadiotext(rds, "Vasco Rossi - Albachiara", false);
    // ARTIST (4) "Vasco Rossi" at 0, TITLE (1) "Albachiara" at 14.
    sendRtPlus(rds, true, 4U, 0U, 11U, 1U, 14U, 10U);
    const core::DlPlusItem item = rds.rtPlus();
    expect(item.artist && item.artist->value() == "Vasco Rossi", "artist");
    expect(item.title && item.title->value() == "Albachiara", "title");
}

void needsTheOdaAnnouncement()
{
    core::RdsMetadataAccumulator rds;
    sendRadiotext(rds, "Vasco Rossi - Albachiara", false);
    sendRtPlus(rds, true, 4U, 0U, 11U, 1U, 14U, 10U);
    expect(!rds.rtPlus().title, "11A ignored without 3A AID 0x4BD7");
}

void itemNotRunningAndNewTextHideTags()
{
    core::RdsMetadataAccumulator rds;
    announceRtPlus(rds);
    sendRadiotext(rds, "Vasco Rossi - Albachiara", false);
    sendRtPlus(rds, false, 4U, 0U, 11U, 1U, 14U, 10U);
    expect(!rds.rtPlus().artist, "item not running");
    sendRtPlus(rds, true, 4U, 0U, 11U, 1U, 14U, 10U);
    rds.applyGroup(kPi, static_cast<std::uint16_t>((2U << 12U) | 0x10U),
                   chars('N', 'e'), chars('w', 's'));
    expect(!rds.rtPlus().artist, "RT A/B flip clears tags");
}

void ebuAccentsBecomeUtf8()
{
    core::RdsMetadataAccumulator rds;
    std::string text = "Citt";
    text.push_back(static_cast<char>(0x81)); // EBU à
    sendRadiotext(rds, text, false);
    const auto rt = rds.radiotext();
    expect(rt && rt->value() == "Citt\xC3\xA0", "RT EBU 0x81 -> U+00E0");
}

} // namespace

int main()
{
    rtPlusSplitsTitleAndArtist();
    needsTheOdaAnnouncement();
    itemNotRunningAndNewTextHideTags();
    ebuAccentsBecomeUtf8();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
