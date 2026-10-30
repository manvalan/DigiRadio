/**
 * @file    MotSlideAssembler.cpp
 * @brief   MotSlideAssembler implementation (EN 301 234 MOT over X-PAD).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/MotSlideAssembler.hpp"

namespace core {

namespace {

constexpr std::size_t kSegmentationHeader = 2U;
constexpr std::size_t kHeaderCore = 7U;
constexpr std::uint8_t kContentTypeImage = 2U;
constexpr std::uint16_t kSubtypeJfif = 1U;
constexpr std::uint16_t kSubtypePng = 3U;

struct HeaderCore {
    std::uint32_t bodySize;
    std::uint8_t contentType;
    std::uint16_t subtype;
};

// EN 301 234 §6.1: BodySize[55:28] HeaderSize[27:15] ContentType[14:9]
// ContentSubType[8:0] over the first 7 header bytes.
[[nodiscard]] HeaderCore parseHeaderCore(std::span<const std::uint8_t> c)
{
    return HeaderCore{
        .bodySize = (static_cast<std::uint32_t>(c[0]) << 20U)
                    | (static_cast<std::uint32_t>(c[1]) << 12U)
                    | (static_cast<std::uint32_t>(c[2]) << 4U)
                    | (static_cast<std::uint32_t>(c[3]) >> 4U),
        .contentType = static_cast<std::uint8_t>((c[5] >> 1U) & 0x3FU),
        .subtype = static_cast<std::uint16_t>(((c[5] & 0x01U) << 8U) | c[6]),
    };
}

// EN 301 234 §5.1.1 segmentation header: RepetitionCount[15:13],
// SegmentSize[12:0]; returns the segment data, or an empty span.
[[nodiscard]] std::span<const std::uint8_t> segmentData(
    std::span<const std::uint8_t> data)
{
    if (data.size() < kSegmentationHeader) {
        return {};
    }
    const std::size_t size =
        (static_cast<std::size_t>(data[0] & 0x1FU) << 8U) | data[1];
    const auto rest = data.subspan(kSegmentationHeader);
    return size <= rest.size() ? rest.first(size)
                               : std::span<const std::uint8_t>{};
}

} // namespace

MotSlideAssembler::MotSlideAssembler(ISlideWriter& writer)
    : writer_(writer)
{
}

void MotSlideAssembler::reset() noexcept
{
    transportId_.reset();
    shownTransportId_.reset();
    segmentSize_ = 0U;
    lastSegment_.reset();
    received_.reset();
}

void MotSlideAssembler::apply(std::uint32_t serviceId,
                              std::span<const std::uint8_t> group)
{
    if (serviceId_ != serviceId) {
        if (serviceId_) {
            writer_.clearSlides();
        }
        reset();
        serviceId_ = serviceId;
    }
    const auto parsed = parseMscDataGroup(group);
    if (!parsed || !parsed->transportId) {
        return;
    }
    if (parsed->type == kDataGroupMotHeader) {
        onHeader(*parsed->transportId, parsed->data);
    } else if (parsed->type == kDataGroupMotBody) {
        onBody(*parsed->transportId, *parsed);
    }
}

void MotSlideAssembler::onHeader(std::uint16_t transportId,
                                 std::span<const std::uint8_t> data)
{
    const auto header = segmentData(data);
    if (header.size() < kHeaderCore || transportId == shownTransportId_
        || transportId == transportId_) {
        return;
    }
    const HeaderCore fields = parseHeaderCore(header);
    const bool image = fields.contentType == kContentTypeImage
                       && (fields.subtype == kSubtypeJfif
                           || fields.subtype == kSubtypePng);
    if (!image || fields.bodySize == 0U
        || fields.bodySize > writer_.maxSlideSize()
        || !writer_.beginSlide(fields.bodySize)) {
        return;
    }
    transportId_ = transportId;
    bodySize_ = fields.bodySize;
    mime_ = fields.subtype == kSubtypePng ? SlideMime::Png : SlideMime::Jpeg;
    segmentSize_ = 0U;
    lastSegment_.reset();
    received_.reset();
}

void MotSlideAssembler::onBody(std::uint16_t transportId,
                               const MscDataGroup& group)
{
    if (transportId != transportId_ || !group.segment
        || *group.segment >= kMaxSegments || received_[*group.segment]
        || (lastSegment_ && *group.segment > *lastSegment_)) {
        return;
    }
    const auto payload = segmentData(group.data);
    const auto offset = offsetOf(*group.segment, group.last,
                                 static_cast<std::uint32_t>(payload.size()));
    if (payload.empty() || !offset) {
        return;
    }
    if (!writer_.writeSlide(*offset, payload)) {
        transportId_.reset();
        return;
    }
    received_.set(*group.segment);
    if (group.last) {
        lastSegment_ = *group.segment;
    }
    if (complete()) {
        writer_.commitSlide(SlideInfo{.id = nextId_++, .mime = mime_,
                                      .size = bodySize_});
        shownTransportId_ = transportId_;
        transportId_.reset();
    }
}

std::optional<std::uint32_t> MotSlideAssembler::offsetOf(
    std::uint16_t segment, bool last, std::uint32_t length)
{
    if (length > bodySize_) {
        return std::nullopt;
    }
    if (last) {
        return bodySize_ - length;
    }
    if (segmentSize_ == 0U) {
        segmentSize_ = length;
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(segment)
                                 * segmentSize_;
    if (length != segmentSize_ || offset + length > bodySize_) {
        return std::nullopt;
    }
    return offset;
}

bool MotSlideAssembler::complete() const noexcept
{
    return lastSegment_
           && received_.count() == static_cast<std::size_t>(*lastSegment_) + 1U;
}

} // namespace core
