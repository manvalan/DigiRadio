/**
 * @file    EmbeddedBlobReader.hpp
 * @brief   IFirmwareBlobReader over a contiguous embedded flash region.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/IFirmwareBlobReader.hpp"

#include <cstddef>
#include <span>

namespace core {

/**
 * @brief    EmbeddedBlobReader — non-owning view of an embedded binary.
 *
 * @dname    EmbeddedBlobReader
 * @return   n/a (type)
 * @pubstate Borrows [data_, data_ + size_) for the reader lifetime.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class EmbeddedBlobReader final : public IFirmwareBlobReader {
public:
    /**
     * @brief    EmbeddedBlobReader — construct over a flash-backed range.
     *
     * @dname    EmbeddedBlobReader
     * @param    data  Start of embedded blob (e.g. linker symbol).
     * @param    size  Length in bytes.
     * @pubstate stores data_ and size_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    EmbeddedBlobReader(const std::byte* data, std::size_t size);

    [[nodiscard]] std::size_t size() const override;

    [[nodiscard]] std::size_t read(std::size_t offset,
                                   std::span<std::byte> dest) const override;

private:
    const std::byte* data_;
    std::size_t size_;
};

} // namespace core
