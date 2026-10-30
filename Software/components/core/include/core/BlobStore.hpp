/**
 * @file    BlobStore.hpp
 * @brief   Layout of the Si4684 image store in otherwise unused flash.
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace core {

/** Images kept in the store, in this order. */
enum class BlobId : std::uint8_t {
    RomPatch = 0U,
    DabImage = 1U,
    FmImage = 2U,
};

inline constexpr std::size_t kBlobCount = 3U;

/** Flash erase unit; the header owns the first sector, images start aligned. */
inline constexpr std::uint32_t kBlobSectorSize = 4096U;

/** Bytes serializeBlobStoreHeader() writes. */
inline constexpr std::size_t kBlobHeaderSize = 64U;

/** Where one image lives inside the store. */
struct BlobEntry {
    std::uint32_t offset{0U}; ///< From the start of the store, sector aligned.
    std::uint32_t size{0U};   ///< Bytes.
    std::uint32_t crc32{0U};  ///< blobCrc32() of the image.

    friend bool operator==(const BlobEntry&, const BlobEntry&) = default;
};

/** The store's table of contents. */
struct BlobStoreHeader {
    std::array<BlobEntry, kBlobCount> entries{}; ///< Indexed by BlobId.

    friend bool operator==(const BlobStoreHeader&,
                           const BlobStoreHeader&) = default;
};

/**
 * @brief    blobCrc32 — CRC-32 (IEEE, reflected), chainable over chunks.
 *
 * @dname    blobCrc32
 * @param    data  Next chunk.
 * @param    crc   Result of the previous chunk; 0 for the first.
 * @return   CRC-32 so far.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::uint32_t blobCrc32(std::span<const std::byte> data,
                                      std::uint32_t crc = 0U) noexcept;

/**
 * @brief    layoutBlobStore — place images back to back after the header.
 *
 * @dname    layoutBlobStore
 * @param    sizes     Image sizes, indexed by BlobId.
 * @param    crcs      Image CRCs, indexed by BlobId.
 * @param    capacity  Store size in bytes.
 * @return   Header, or nullopt when an image is empty or they do not fit.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<BlobStoreHeader> layoutBlobStore(
    const std::array<std::uint32_t, kBlobCount>& sizes,
    const std::array<std::uint32_t, kBlobCount>& crcs,
    std::uint32_t capacity) noexcept;

/**
 * @brief    serializeBlobStoreHeader — header bytes for the first sector.
 *
 * Little-endian: magic "DRSI4684", format version, entries, CRC-32 of the
 * preceding bytes.
 *
 * @dname    serializeBlobStoreHeader
 * @param    header  Table of contents.
 * @return   kBlobHeaderSize bytes.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::array<std::byte, kBlobHeaderSize> serializeBlobStoreHeader(
    const BlobStoreHeader& header) noexcept;

/**
 * @brief    parseBlobStoreHeader — read back the first sector.
 *
 * @dname    parseBlobStoreHeader
 * @param    bytes     At least kBlobHeaderSize bytes from the store start.
 * @param    capacity  Store size in bytes.
 * @return   Header, or nullopt for erased flash, a bad magic, version or
 *           checksum, or entries outside the store.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-26
 */
[[nodiscard]] std::optional<BlobStoreHeader> parseBlobStoreHeader(
    std::span<const std::byte> bytes, std::uint32_t capacity) noexcept;

} // namespace core
