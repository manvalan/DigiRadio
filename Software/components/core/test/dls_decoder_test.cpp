/**
 * @file    dls_decoder_test.cpp
 * @brief   Host tests for DlsDecoder (AN649 DLS prefix, DL Plus tags).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/DlsDecoder.hpp"
#include "core/TunerJson.hpp"
#include "core/TunerStatus.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

void expect(bool ok, std::string_view what)
{
    if (!ok) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

// DL message: Toggle[7] C=0, Charset[7:4] in byte 1 (AN649 Table 22).
std::vector<std::uint8_t> message(bool toggle, std::uint8_t charset,
                                  std::string_view text)
{
    std::vector<std::uint8_t> out = {
        static_cast<std::uint8_t>(toggle ? 0x80U : 0x00U),
        static_cast<std::uint8_t>(charset << 4U)};
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

// DL Plus command (AN649 Table 23): C=1, Command 0010, Link[4], then
// CId=0 IT IR NT and 3-byte tags.
std::vector<std::uint8_t> dlPlus(bool link, bool running,
                                 const std::vector<std::uint8_t>& tags)
{
    const auto count = static_cast<std::uint8_t>(tags.size() / 3U - 1U);
    std::vector<std::uint8_t> out = {
        0x12U, static_cast<std::uint8_t>(link ? 0x10U : 0x00U),
        static_cast<std::uint8_t>((running ? 0x04U : 0x00U) | count)};
    out.insert(out.end(), tags.begin(), tags.end());
    return out;
}

std::string labelOf(const core::DlsDecoder& dls)
{
    const auto label = dls.label();
    return label ? std::string(label->value()) : std::string("<none>");
}

void prefixNeverReachesLabel()
{
    core::DlsDecoder dls;
    dls.apply(message(false, 0U, "Radio Uno"));
    expect(labelOf(dls) == "Radio Uno", "EBU message without prefix bytes");
}

void ebuLatinAccentsBecomeUtf8()
{
    core::DlsDecoder dls;
    std::vector<std::uint8_t> payload = message(true, 0U, "Citt");
    payload.push_back(0x81U); // EBU Latin à
    dls.apply(payload);
    expect(labelOf(dls) == "Citt\xC3\xA0", "EBU 0x81 decodes to U+00E0");
}

void utf8MessagePassesThrough()
{
    core::DlsDecoder dls;
    dls.apply(message(false, 15U, "Perch\xC3\xA9"));
    expect(labelOf(dls) == "Perch\xC3\xA9", "UTF-8 charset kept");
}

void dlPlusResolvesTitleAndArtist()
{
    core::DlsDecoder dls;
    dls.apply(message(true, 0U, "Vasco Rossi - Albachiara"));
    // ARTIST (4) at 0 length 11; TITLE (1) at 14 length 10.
    dls.apply(dlPlus(true, true, {4U, 0U, 10U, 1U, 14U, 9U}));
    const core::DlPlusItem item = dls.item();
    expect(item.artist && item.artist->value() == "Vasco Rossi",
           "DL Plus artist");
    expect(item.title && item.title->value() == "Albachiara",
           "DL Plus title");
}

void dlPlusWithStaleLinkIsIgnored()
{
    core::DlsDecoder dls;
    dls.apply(message(false, 0U, "News at nine"));
    dls.apply(dlPlus(true, true, {1U, 0U, 3U}));
    expect(!dls.item().title, "tags whose Link != Toggle are not applied");
}

void dlPlusItemNotRunningIsHidden()
{
    core::DlsDecoder dls;
    dls.apply(message(true, 0U, "Jingle"));
    dls.apply(dlPlus(true, false, {1U, 0U, 5U}));
    expect(!dls.item().title, "IR=0 hides ITEM tags");
}

void removeLabelCommandClears()
{
    core::DlsDecoder dls;
    dls.apply(message(false, 0U, "Hello"));
    dls.apply(std::vector<std::uint8_t>{0x11U, 0x00U});
    expect(!dls.label(), "remove-label command clears the label");
}

void malformedPayloadIsIgnored()
{
    core::DlsDecoder dls;
    dls.apply(message(false, 0U, "Keep"));
    dls.apply(std::vector<std::uint8_t>{0x12U});
    dls.apply(dlPlus(false, true, {1U, 0U}));
    expect(labelOf(dls) == "Keep", "short payloads leave state unchanged");
}

void statusJsonCarriesDlPlusAndEscapesControls()
{
    core::TunerStatus status = {};
    status.band = core::TunerBand::Dab;
    status.dabDynamicLabel = core::BroadcastLabel::tryFromChipBytes(
        std::string_view("A\x01" "B", 3));
    status.dabDlPlusTitle = core::BroadcastLabel::tryFromChipBytes("T");
    const std::string json = core::serializeTunerStatusJson(status);
    expect(json.find(R"("dynamic_label":"A\u0001B")") != std::string::npos,
           "control characters are \\u escaped");
    expect(json.find(R"("dl_plus":{"title":"T"})") != std::string::npos,
           "dl_plus object with title only");
}

} // namespace

int main()
{
    prefixNeverReachesLabel();
    ebuLatinAccentsBecomeUtf8();
    utf8MessagePassesThrough();
    dlPlusResolvesTitleAndArtist();
    dlPlusWithStaleLinkIsIgnored();
    dlPlusItemNotRunningIsHidden();
    removeLabelCommandClears();
    malformedPayloadIsIgnored();
    statusJsonCarriesDlPlusAndEscapesControls();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
