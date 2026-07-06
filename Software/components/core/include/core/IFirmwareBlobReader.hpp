/**
 * @file    IFirmwareBlobReader.hpp
 * @brief   Read-only streaming access to a firmware blob in flash.
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

#include <cstddef>
#include <span>

namespace core {

/**
 * @brief    IFirmwareBlobReader — streaming firmware image access.
 *
 * @dname    IFirmwareBlobReader
 * @return   n/a (type)
 * @pubstate Implementations wrap embedded flash or test buffers. Used by
 *           Si4684Driver to HOST_LOAD in bounded chunks (AGENTS.md §7.1).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class IFirmwareBlobReader {
public:
    /**
     * @brief    ~IFirmwareBlobReader — virtual destructor.
     *
     * @dname    ~IFirmwareBlobReader
     * @pubstate n/a
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    virtual ~IFirmwareBlobReader() = default;

    /**
     * @brief    size — total blob length in bytes.
     *
     * @dname    size
     * @return   Blob size.
     * @pubstate reads backing storage.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    /**
     * @brief    read — copy bytes from the blob at an offset.
     *
     * @dname    read
     * @param    offset  Byte offset from the start of the blob.
     * @param    dest    Destination buffer; truncated at end of blob.
     * @return   Number of bytes copied into dest.
     * @pubstate reads backing storage.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::size_t read(std::size_t offset,
                                           std::span<std::byte> dest) const = 0;
};

} // namespace core
