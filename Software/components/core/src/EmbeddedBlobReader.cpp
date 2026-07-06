/**
 * @file    EmbeddedBlobReader.cpp
 * @brief   EmbeddedBlobReader implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/EmbeddedBlobReader.hpp"

#include <algorithm>
#include <cstring>

namespace core {

EmbeddedBlobReader::EmbeddedBlobReader(const std::byte* data, std::size_t size)
    : data_(data)
    , size_(size)
{
}

std::size_t EmbeddedBlobReader::size() const
{
    return size_;
}

std::size_t EmbeddedBlobReader::read(std::size_t offset,
                                     std::span<std::byte> dest) const
{
    if (offset >= size_ || dest.empty()) {
        return 0;
    }
    const std::size_t available = size_ - offset;
    const std::size_t toCopy = std::min(available, dest.size());
    std::memcpy(dest.data(), data_ + offset, toCopy);
    return toCopy;
}

} // namespace core
