/**
 * @file    ISlideWriter.hpp
 * @brief   Storage for assembled DAB slideshow images.
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

#include <cstdint>
#include <span>

namespace core {

/** Image formats the slideshow keeps (EN 301 234 content type 2). */
enum class SlideMime : std::uint8_t {
    Jpeg, ///< Subtype 1, JFIF.
    Png,  ///< Subtype 3.
};

/**
 * @brief    SlideInfo — a complete slide ready to be served.
 *
 * @dname    SlideInfo
 * @return   n/a (type)
 * @pubstate Plain DTO.
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
struct SlideInfo {
    std::uint32_t id{0U};           ///< Changes with every new slide (≥ 1).
    SlideMime mime{SlideMime::Jpeg}; ///< Image format.
    std::uint32_t size{0U};         ///< Bytes.
};

/**
 * @brief    ISlideWriter — where MotSlideAssembler puts image bytes.
 *
 * @dname    ISlideWriter
 * @param    none
 * @return   n/a (interface)
 * @pubstate Public interface: maxSlideSize(), beginSlide(), writeSlide(),
 *           commitSlide(), clearSlides(). The device implementation
 *           writes flash, so a slide never has to fit in RAM.
 *
 * Writes for one slide arrive in any order; the slide being written must
 * not replace the one already committed until commitSlide().
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
class ISlideWriter {
public:
    virtual ~ISlideWriter() = default;

    /**
     * @brief    maxSlideSize — largest slide the store accepts.
     *
     * @dname    maxSlideSize
     * @return   Bytes.
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] virtual std::uint32_t maxSlideSize() const noexcept = 0;

    /**
     * @brief    beginSlide — start a new slide of size bytes.
     *
     * @dname    beginSlide
     * @param    size  Body size from the MOT header (≤ maxSlideSize()).
     * @return   false when the store cannot take it.
     * @pubstate implementation-defined; abandons an uncommitted slide.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] virtual bool beginSlide(std::uint32_t size) = 0;

    /**
     * @brief    writeSlide — store bytes of the slide being assembled.
     *
     * @dname    writeSlide
     * @param    offset  Position in the body.
     * @param    bytes   Data (offset + size ≤ the begun size).
     * @return   false on a storage error.
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] virtual bool writeSlide(
        std::uint32_t offset, std::span<const std::uint8_t> bytes) = 0;

    /**
     * @brief    commitSlide — publish the slide just completed.
     *
     * @dname    commitSlide
     * @param    info  Id, format and size of the finished slide.
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    virtual void commitSlide(const SlideInfo& info) = 0;

    /**
     * @brief    clearSlides — forget the published slide (service change).
     *
     * @dname    clearSlides
     * @pubstate implementation-defined.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    virtual void clearSlides() = 0;
};

} // namespace core
