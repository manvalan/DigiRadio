/**
 * @file    Adau1452Store.cpp
 * @brief   Two checked slots in the "dsp" partition.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-01
 */

#include "adau1452/Adau1452Store.hpp"

#include "adau1452/Adau1452Eeprom.hpp"
#include "adau1452/Adau1452Images.hpp"

#include "core/BlobStore.hpp"
#include "core/DspProgramBlob.hpp"

#include "esp_log.h"
#include "esp_partition.h"

#include <cstddef>

namespace adau1452::store {

namespace {

constexpr char kTag[] = "Adau1452Store";
constexpr std::uint32_t kMagic = 0x32564452U; // "RDV2"
constexpr std::size_t kSectorBytes = 4096U;
constexpr esp_partition_subtype_t kSubtype =
    static_cast<esp_partition_subtype_t>(0x40U);

struct Slot {
    std::size_t offset;
    std::size_t capacity;
};
constexpr Slot kProgramSlot{0x00000U, 0x10000U};
constexpr Slot kImageSlot{0x10000U, 0x30000U};

struct Header {
    std::uint32_t magic;
    std::uint32_t size;
    std::uint32_t crc;
};

[[nodiscard]] const esp_partition_t* partition()
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, kSubtype, "dsp");
}

[[nodiscard]] bool eraseSlot(const esp_partition_t* part, Slot slot)
{
    return esp_partition_erase_range(part, slot.offset, slot.capacity) == ESP_OK;
}

[[nodiscard]] bool writeSlot(Slot slot, std::span<const std::uint8_t> data)
{
    const esp_partition_t* part = partition();
    if (part == nullptr || data.size() + kSectorBytes > slot.capacity) {
        return false;
    }
    if (!eraseSlot(part, slot)
        || esp_partition_write(part, slot.offset + kSectorBytes, data.data(),
                               data.size()) != ESP_OK) {
        return false;
    }
    // Header last: a power cut leaves an empty slot, not a damaged one.
    const Header header{kMagic, static_cast<std::uint32_t>(data.size()),
                        core::blobCrc32(std::as_bytes(data), 0U)};
    return esp_partition_write(part, slot.offset, &header, sizeof(header))
           == ESP_OK;
}

[[nodiscard]] std::vector<std::uint8_t> readSlot(Slot slot)
{
    const esp_partition_t* part = partition();
    Header header{};
    if (part == nullptr
        || esp_partition_read(part, slot.offset, &header, sizeof(header))
               != ESP_OK
        || header.magic != kMagic
        || header.size + kSectorBytes > slot.capacity) {
        return {};
    }
    std::vector<std::uint8_t> data(header.size);
    if (esp_partition_read(part, slot.offset + kSectorBytes, data.data(),
                           data.size()) != ESP_OK
        || core::blobCrc32(std::as_bytes(std::span<const std::uint8_t>(data)),
                           0U) != header.crc) {
        ESP_LOGW(kTag, "stored DSP data damaged: ignored");
        return {};
    }
    return data;
}

} // namespace

bool saveProgram(std::span<const std::uint8_t> blob)
{
    return writeSlot(kProgramSlot, blob);
}

bool clearProgram()
{
    const esp_partition_t* part = partition();
    return part != nullptr && eraseSlot(part, kProgramSlot);
}

bool hasProgram()
{
    return !readSlot(kProgramSlot).empty();
}

bool saveEepromImage(std::span<const std::uint8_t> image)
{
    if (!writeSlot(kImageSlot, image) || !clearProgram()) {
        return false;
    }
    forgetWrittenEepromImage();
    return true;
}

std::vector<std::uint8_t> loadEepromImage()
{
    return readSlot(kImageSlot);
}

std::expected<core::DspProgram, core::DspProgramError>
ProgramSource::loadProgram()
{
    const std::vector<std::uint8_t> stored = readSlot(kProgramSlot);
    if (!stored.empty()) {
        if (auto program = core::parseDspProgramBlob(stored); program) {
            return program;
        }
        ESP_LOGW(kTag, "uploaded program unreadable: using the one in the app");
    }
    return EmbeddedProgramSource{}.loadProgram();
}

} // namespace adau1452::store
