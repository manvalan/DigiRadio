/**
 * @file    Si4684ImageStore.cpp
 * @brief   Si4684 firmware images kept in unpartitioned flash.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "si4684/Si4684ImageStore.hpp"

#include "esp_flash.h"
#include "esp_log.h"

#include <algorithm>
#include <memory>
#include <optional>

namespace si4684 {

namespace {

constexpr char kTag[] = "si4684_store";
constexpr char kLabel[] = "si4684img";
constexpr std::size_t kChunk = core::kBlobSectorSize;

using Readers = std::array<const core::IFirmwareBlobReader*, core::kBlobCount>;

/** CRC of a whole image, read in sector-sized chunks. */
[[nodiscard]] std::optional<std::uint32_t> crcOf(
    const core::IFirmwareBlobReader& reader, std::span<std::byte> buffer)
{
    std::uint32_t crc = 0U;
    for (std::size_t at = 0U; at < reader.size();) {
        const std::size_t got = reader.read(at, buffer);
        if (got == 0U) {
            return std::nullopt;
        }
        crc = core::blobCrc32(buffer.first(got), crc);
        at += got;
    }
    return crc;
}

[[nodiscard]] std::optional<core::BlobStoreHeader> headerFor(
    const Readers& sources, std::span<std::byte> buffer)
{
    std::array<std::uint32_t, core::kBlobCount> sizes{};
    std::array<std::uint32_t, core::kBlobCount> crcs{};
    for (std::size_t i = 0U; i < core::kBlobCount; ++i) {
        const auto crc = crcOf(*sources[i], buffer);
        if (!crc) {
            return std::nullopt;
        }
        sizes[i] = static_cast<std::uint32_t>(sources[i]->size());
        crcs[i] = *crc;
    }
    return core::layoutBlobStore(sizes, crcs, Si4684ImageStore::kCapacity);
}

[[nodiscard]] bool copyImage(const esp_partition_t* part,
                             const core::IFirmwareBlobReader& source,
                             std::uint32_t offset, std::span<std::byte> buffer)
{
    for (std::size_t at = 0U; at < source.size();) {
        const std::size_t got = source.read(at, buffer);
        if (got == 0U
            || esp_partition_write(part, offset + at, buffer.data(), got) != ESP_OK) {
            return false;
        }
        at += got;
    }
    return true;
}

[[nodiscard]] std::uint32_t storeEnd(const core::BlobStoreHeader& header)
{
    const auto& last = header.entries.back();
    const std::uint32_t end = last.offset + last.size;
    return (end + core::kBlobSectorSize - 1U) / core::kBlobSectorSize
           * core::kBlobSectorSize;
}

} // namespace

void FlashBlobReader::bind(const esp_partition_t* partition,
                           core::BlobEntry entry) noexcept
{
    partition_ = partition;
    entry_ = entry;
}

std::size_t FlashBlobReader::size() const
{
    return partition_ == nullptr ? 0U : entry_.size;
}

std::size_t FlashBlobReader::read(std::size_t offset,
                                  std::span<std::byte> dest) const
{
    if (partition_ == nullptr || offset >= entry_.size || dest.empty()) {
        return 0U;
    }
    const std::size_t count = std::min<std::size_t>(dest.size(), entry_.size - offset);
    if (esp_partition_read(partition_, entry_.offset + offset, dest.data(), count)
        != ESP_OK) {
        return 0U;
    }
    return count;
}

const esp_partition_t* Si4684ImageStore::partition()
{
    if (partition_ == nullptr
        && esp_partition_register_external(
               esp_flash_default_chip, kFlashOffset, kCapacity, kLabel,
               ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_UNDEFINED,
               &partition_)
               != ESP_OK) {
        ESP_LOGE(kTag, "cannot register flash at 0x%lx",
                 static_cast<unsigned long>(kFlashOffset));
        partition_ = nullptr;
    }
    return partition_;
}

bool Si4684ImageStore::open()
{
    const esp_partition_t* part = partition();
    auto buffer = std::make_unique<std::byte[]>(kChunk);
    const std::span<std::byte> chunk(buffer.get(), kChunk);
    std::optional<core::BlobStoreHeader> header;
    if (part != nullptr
        && esp_partition_read(part, 0, chunk.data(), core::kBlobHeaderSize) == ESP_OK) {
        header = core::parseBlobStoreHeader(chunk.first(core::kBlobHeaderSize),
                                            kCapacity);
    }
    for (std::size_t i = 0U; header && i < core::kBlobCount; ++i) {
        readers_[i].bind(part, header->entries[i]);
        if (crcOf(readers_[i], chunk) != header->entries[i].crc32) {
            header.reset();
        }
    }
    if (!header) {
        for (auto& reader : readers_) {
            reader.bind(nullptr, {});
        }
        state_ = State::Missing;
        return false;
    }
    state_ = State::Ready;
    return true;
}

bool Si4684ImageStore::fillFrom(const core::IFirmwareBlobReader& patch,
                                const core::IFirmwareBlobReader& dab,
                                const core::IFirmwareBlobReader& fm)
{
    auto buffer = std::make_unique<std::byte[]>(kChunk);
    const std::span<std::byte> chunk(buffer.get(), kChunk);
    const Readers sources{&patch, &dab, &fm};
    const auto wanted = headerFor(sources, chunk);
    const esp_partition_t* part = partition();
    if (!wanted || part == nullptr) {
        state_ = State::Missing;
        return false;
    }
    if (open() && readers_[0].size() == wanted->entries[0].size) {
        std::array<std::byte, core::kBlobHeaderSize> stored{};
        if (esp_partition_read(part, 0, stored.data(), stored.size()) == ESP_OK
            && core::parseBlobStoreHeader(stored, kCapacity) == wanted) {
            return true; // already there
        }
    }
    state_ = State::Writing;
    ESP_LOGI(kTag, "copying Si4684 images to flash (once)");
    bool ok = esp_partition_erase_range(part, 0, storeEnd(*wanted)) == ESP_OK;
    for (std::size_t i = 0U; ok && i < core::kBlobCount; ++i) {
        ok = copyImage(part, *sources[i], wanted->entries[i].offset, chunk);
    }
    const auto bytes = core::serializeBlobStoreHeader(*wanted);
    ok = ok && esp_partition_write(part, 0, bytes.data(), bytes.size()) == ESP_OK;
    ok = ok && open();
    ESP_LOGI(kTag, "Si4684 image store %s", ok ? "ready" : "FAILED");
    return ok;
}

Si4684ImageStore::State Si4684ImageStore::state() const noexcept
{
    return state_;
}

const char* Si4684ImageStore::stateToken() const noexcept
{
    switch (state_.load()) {
    case State::Missing:
        return "missing";
    case State::Writing:
        return "writing";
    case State::Ready:
        return "ready";
    case State::Unknown:
        break;
    }
    return "unknown";
}

const core::IFirmwareBlobReader& Si4684ImageStore::romPatch() const noexcept
{
    return readers_[static_cast<std::size_t>(core::BlobId::RomPatch)];
}

const core::IFirmwareBlobReader& Si4684ImageStore::dabFirmware() const noexcept
{
    return readers_[static_cast<std::size_t>(core::BlobId::DabImage)];
}

const core::IFirmwareBlobReader& Si4684ImageStore::fmFirmware() const noexcept
{
    return readers_[static_cast<std::size_t>(core::BlobId::FmImage)];
}

} // namespace si4684
