/**
 * @file    MotSlideAssembler.hpp
 * @brief   Rebuild MOT slideshow images from X-PAD data groups.
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
#pragma once

#include "core/ISlideWriter.hpp"
#include "core/MscDataGroup.hpp"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace core {

/**
 * @brief    MotSlideAssembler — MOT header/body segments to whole slides.
 *
 * @dname    MotSlideAssembler
 * @param    writer  Slide storage, borrowed for the object's lifetime.
 * @return   n/a (type)
 * @pubstate Public interface: apply(serviceId, group), reset(). Tracks
 *           one object at a time: its transport id, MOT header fields and
 *           which body segments arrived.
 *
 * The Si468x forwards MOT data groups as they arrive (AN649 §7.11). A
 * header group (type 3) carries the MOT header core (EN 301 234 §6.1:
 * BodySize 28 bits, HeaderSize 13, ContentType 6, ContentSubType 9);
 * body groups (type 4) carry segments behind a 2-byte segmentation
 * header (repetition 3 bits, segment size 13). All segments but the last
 * have the same size, so each one is written at segment × size (the last
 * at BodySize − its size) as soon as it arrives. Only JPEG/PNG images up
 * to writer.maxSlideSize() are kept; a repeated transport id of the slide
 * already shown is ignored, so carousel repetitions do not rewrite flash.
 * A new service id clears everything.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class MotSlideAssembler {
public:
    /** Body segments tracked per object (a 64 KB slide in ≥ 64-byte parts). */
    static constexpr std::size_t kMaxSegments = 1024U;

    /**
     * @brief    MotSlideAssembler — bind to the slide store.
     *
     * @dname    MotSlideAssembler
     * @param    writer  Storage that outlives the assembler.
     * @pubstate stores the reference; nothing in progress.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    explicit MotSlideAssembler(ISlideWriter& writer);

    /**
     * @brief    apply — process one MOT data group.
     *
     * @dname    apply
     * @param    serviceId  Service the PAD came with.
     * @param    group      Raw data group (CRC checked when present).
     * @pubstate may start, extend, complete (commitSlide) or abandon the
     *           object in progress; malformed groups are dropped.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void apply(std::uint32_t serviceId, std::span<const std::uint8_t> group);

    /**
     * @brief    reset — drop the object in progress and the repeat filter.
     *
     * @dname    reset
     * @pubstate clears the assembly state (the writer keeps its slide).
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    void reset() noexcept;

private:
    void onHeader(std::uint16_t transportId, std::span<const std::uint8_t> data);
    void onBody(std::uint16_t transportId, const MscDataGroup& group);
    [[nodiscard]] std::optional<std::uint32_t> offsetOf(
        std::uint16_t segment, bool last, std::uint32_t length);
    [[nodiscard]] bool complete() const noexcept;

    ISlideWriter& writer_;
    std::optional<std::uint32_t> serviceId_;
    std::optional<std::uint16_t> transportId_;
    std::optional<std::uint16_t> shownTransportId_;
    std::uint32_t bodySize_{0U};
    SlideMime mime_{SlideMime::Jpeg};
    std::uint32_t segmentSize_{0U};
    std::optional<std::uint16_t> lastSegment_;
    std::bitset<kMaxSegments> received_;
    std::uint32_t nextId_{1U};
};

} // namespace core
