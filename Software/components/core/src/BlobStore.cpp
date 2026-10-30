/**
 * @file    BlobStore.cpp
 * @brief   Layout of the Si4684 image store in otherwise unused flash.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/BlobStore.hpp"

#include <cstring>

namespace core {

namespace {

constexpr std::array<char, 8> kMagic{'D', 'R', 'S', 'I', '4', '6', '8', '4'};
constexpr std::uint32_t kFormatVersion = 1U;
constexpr std::size_t kVersionAt = 8U;
constexpr std::size_t kEntriesAt = 12U;
constexpr std::size_t kEntryBytes = 12U;
constexpr std::size_t kCrcAt = kEntriesAt + kBlobCount * kEntryBytes;

static_assert(kCrcAt + 4U <= kBlobHeaderSize);

void putU32(std::span<std::byte> out, std::size_t at, std::uint32_t value)
{
    for (std::size_t i = 0U; i < 4U; ++i) {
        out[at + i] = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
}

[[nodiscard]] std::uint32_t getU32(std::span<const std::byte> in, std::size_t at)
{
    std::uint32_t value = 0U;
    for (std::size_t i = 0U; i < 4U; ++i) {
        value |= std::to_integer<std::uint32_t>(in[at + i]) << (8U * i);
    }
    return value;
}

[[nodiscard]] std::uint32_t alignUp(std::uint32_t value)
{
    return (value + kBlobSectorSize - 1U) / kBlobSectorSize * kBlobSectorSize;
}

[[nodiscard]] bool entryFits(const BlobEntry& entry, std::uint32_t capacity)
{
    return entry.size > 0U && entry.offset >= kBlobSectorSize
           && entry.offset % kBlobSectorSize == 0U && entry.offset < capacity
           && entry.size <= capacity - entry.offset;
}

} // namespace

std::uint32_t blobCrc32(std::span<const std::byte> data,
                        std::uint32_t crc) noexcept
{
    crc = ~crc;
    for (const std::byte b : data) {
        crc ^= std::to_integer<std::uint32_t>(b);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
        }
    }
    return ~crc;
}

std::optional<BlobStoreHeader> layoutBlobStore(
    const std::array<std::uint32_t, kBlobCount>& sizes,
    const std::array<std::uint32_t, kBlobCount>& crcs,
    std::uint32_t capacity) noexcept
{
    BlobStoreHeader header;
    std::uint32_t next = kBlobSectorSize;
    for (std::size_t i = 0U; i < kBlobCount; ++i) {
        header.entries[i] = BlobEntry{.offset = next, .size = sizes[i], .crc32 = crcs[i]};
        if (!entryFits(header.entries[i], capacity)) {
            return std::nullopt;
        }
        next = alignUp(next + sizes[i]);
    }
    return header;
}

std::array<std::byte, kBlobHeaderSize> serializeBlobStoreHeader(
    const BlobStoreHeader& header) noexcept
{
    std::array<std::byte, kBlobHeaderSize> out{};
    std::memcpy(out.data(), kMagic.data(), kMagic.size());
    putU32(out, kVersionAt, kFormatVersion);
    for (std::size_t i = 0U; i < kBlobCount; ++i) {
        const std::size_t at = kEntriesAt + i * kEntryBytes;
        putU32(out, at, header.entries[i].offset);
        putU32(out, at + 4U, header.entries[i].size);
        putU32(out, at + 8U, header.entries[i].crc32);
    }
    putU32(out, kCrcAt, blobCrc32(std::span(out).first(kCrcAt)));
    return out;
}

std::optional<BlobStoreHeader> parseBlobStoreHeader(
    std::span<const std::byte> bytes, std::uint32_t capacity) noexcept
{
    if (bytes.size() < kBlobHeaderSize
        || std::memcmp(bytes.data(), kMagic.data(), kMagic.size()) != 0
        || getU32(bytes, kVersionAt) != kFormatVersion
        || getU32(bytes, kCrcAt) != blobCrc32(bytes.first(kCrcAt))) {
        return std::nullopt;
    }
    BlobStoreHeader header;
    for (std::size_t i = 0U; i < kBlobCount; ++i) {
        const std::size_t at = kEntriesAt + i * kEntryBytes;
        header.entries[i] = BlobEntry{.offset = getU32(bytes, at),
                                      .size = getU32(bytes, at + 4U),
                                      .crc32 = getU32(bytes, at + 8U)};
        if (!entryFits(header.entries[i], capacity)) {
            return std::nullopt;
        }
    }
    return header;
}

} // namespace core
