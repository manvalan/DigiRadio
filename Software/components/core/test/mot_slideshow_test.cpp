/**
 * @file    mot_slideshow_test.cpp
 * @brief   Host tests for MSC data groups and MOT slide assembly.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/MotSlideAssembler.hpp"
#include "core/MscDataGroup.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
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

class FakeWriter : public core::ISlideWriter {
public:
    [[nodiscard]] std::uint32_t maxSlideSize() const noexcept override
    {
        return 65536U;
    }
    [[nodiscard]] bool beginSlide(std::uint32_t size) override
    {
        ++begins;
        pending.assign(size, 0U);
        return true;
    }
    [[nodiscard]] bool writeSlide(std::uint32_t offset,
                                  std::span<const std::uint8_t> bytes) override
    {
        std::copy(bytes.begin(), bytes.end(), pending.begin() + offset);
        return true;
    }
    void commitSlide(const core::SlideInfo& info) override
    {
        shown = pending;
        last = info;
    }
    void clearSlides() override
    {
        shown.clear();
        last.reset();
    }

    std::vector<std::uint8_t> pending;
    std::vector<std::uint8_t> shown;
    std::optional<core::SlideInfo> last;
    int begins = 0;
};

// Data group with CRC, segment field and user access carrying a TID
// (EN 300 401 §5.3.3), wrapping a MOT segment (2-byte segmentation header).
std::vector<std::uint8_t> dataGroup(std::uint8_t type, std::uint16_t segment,
                                    bool last, std::uint16_t tid,
                                    const std::vector<std::uint8_t>& seg)
{
    std::vector<std::uint8_t> g = {
        static_cast<std::uint8_t>(0x40U | 0x20U | 0x10U | type), 0x00U,
        static_cast<std::uint8_t>((last ? 0x80U : 0x00U) | (segment >> 8U)),
        static_cast<std::uint8_t>(segment & 0xFFU),
        0x12U, // TID flag, length 2
        static_cast<std::uint8_t>(tid >> 8U),
        static_cast<std::uint8_t>(tid & 0xFFU),
        static_cast<std::uint8_t>(seg.size() >> 8U),
        static_cast<std::uint8_t>(seg.size() & 0xFFU)};
    g.insert(g.end(), seg.begin(), seg.end());
    const std::uint16_t crc = core::crc16Ccitt(g);
    g.push_back(static_cast<std::uint8_t>(crc >> 8U));
    g.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
    return g;
}

// MOT header core: BodySize 28, HeaderSize 13 (7), ContentType 6, Subtype 9.
std::vector<std::uint8_t> header(std::uint32_t bodySize, std::uint8_t type,
                                 std::uint16_t subtype)
{
    const std::uint16_t headerSize = 7U;
    return {static_cast<std::uint8_t>(bodySize >> 20U),
            static_cast<std::uint8_t>(bodySize >> 12U),
            static_cast<std::uint8_t>(bodySize >> 4U),
            static_cast<std::uint8_t>(((bodySize & 0x0FU) << 4U)
                                      | (headerSize >> 9U)),
            static_cast<std::uint8_t>(headerSize >> 1U),
            static_cast<std::uint8_t>(((headerSize & 1U) << 7U) | (type << 1U)
                                      | (subtype >> 8U)),
            static_cast<std::uint8_t>(subtype & 0xFFU)};
}

void crcMatchesKnownValue()
{
    const std::vector<std::uint8_t> ascii = {'1', '2', '3', '4', '5',
                                             '6', '7', '8', '9'};
    // CRC-16/GENIBUS (CCITT, preset 0xFFFF, inverted) check value.
    expect(core::crc16Ccitt(ascii) == 0xD64EU, "CRC-16 check value 0xD64E");
}

void parsesSessionHeader()
{
    const auto g = dataGroup(4U, 3U, true, 0xBEEFU, {1U, 2U, 3U});
    const auto parsed = core::parseMscDataGroup(g);
    expect(parsed && parsed->type == 4U && parsed->segment == 3U
               && parsed->last && parsed->transportId == 0xBEEFU
               && parsed->data.size() == 5U,
           "type, segment, last, TID, data");
    auto corrupt = g;
    corrupt[8] ^= 0x01U;
    expect(!core::parseMscDataGroup(corrupt), "bad CRC rejected");
}

void assemblesOutOfOrderSegments()
{
    FakeWriter writer;
    core::MotSlideAssembler slides(writer);
    const std::vector<std::uint8_t> body = {10, 11, 12, 13, 14, 15, 16};
    slides.apply(1U, dataGroup(3U, 0U, true, 7U, header(7U, 2U, 1U)));
    slides.apply(1U, dataGroup(4U, 2U, true, 7U, {16}));
    slides.apply(1U, dataGroup(4U, 0U, false, 7U, {10, 11, 12}));
    expect(!writer.last, "not complete with segment 1 missing");
    slides.apply(1U, dataGroup(4U, 1U, false, 7U, {13, 14, 15}));
    expect(writer.last && writer.last->id == 1U
               && writer.last->mime == core::SlideMime::Jpeg
               && writer.shown == body,
           "JPEG slide assembled in order");
}

void repeatedSlideIsNotRewritten()
{
    FakeWriter writer;
    core::MotSlideAssembler slides(writer);
    slides.apply(1U, dataGroup(3U, 0U, true, 9U, header(2U, 2U, 3U)));
    slides.apply(1U, dataGroup(4U, 0U, true, 9U, {1, 2}));
    slides.apply(1U, dataGroup(3U, 0U, true, 9U, header(2U, 2U, 3U)));
    expect(writer.begins == 1 && writer.last
               && writer.last->mime == core::SlideMime::Png,
           "carousel repetition ignored");
}

void nonImagesAndOversizeAreIgnored()
{
    FakeWriter writer;
    core::MotSlideAssembler slides(writer);
    slides.apply(1U, dataGroup(3U, 0U, true, 1U, header(10U, 1U, 0U)));
    slides.apply(1U, dataGroup(3U, 0U, true, 2U, header(70000U, 2U, 1U)));
    expect(writer.begins == 0, "text object and 70 KB image skipped");
}

void serviceChangeClearsTheSlide()
{
    FakeWriter writer;
    core::MotSlideAssembler slides(writer);
    slides.apply(1U, dataGroup(3U, 0U, true, 9U, header(1U, 2U, 1U)));
    slides.apply(1U, dataGroup(4U, 0U, true, 9U, {5}));
    slides.apply(2U, dataGroup(4U, 0U, true, 9U, {5}));
    expect(!writer.last && writer.shown.empty(), "new service clears");
}

} // namespace

int main()
{
    crcMatchesKnownValue();
    parsesSessionHeader();
    assemblesOutOfOrderSegments();
    repeatedSlideIsNotRewritten();
    nonImagesAndOversizeAreIgnored();
    serviceChangeClearsTheSlide();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
