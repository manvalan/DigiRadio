/**
 * @file    FlashDspProgramSource.cpp
 * @brief   FlashDspProgramSource implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "adau1701/FlashDspProgramSource.hpp"

#include "core/DspProgramBlob.hpp"

#include "esp_log.h"
#include "esp_partition.h"

#include <vector>

namespace adau1701 {

namespace {

constexpr char kTag[] = "FlashDsp";
constexpr std::uint8_t kDspPartitionSubtype = 0x40U;

[[nodiscard]] const esp_partition_t* dspPartition()
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    kDspPartitionSubtype,
                                    "dsp");
}

[[nodiscard]] bool partitionLooksEmpty(std::span<const std::uint8_t> data)
{
    for (const std::uint8_t byte : data) {
        if (byte != 0xFFU) {
            return false;
        }
    }
    return true;
}

} // namespace

std::expected<core::DspProgram, core::DspProgramError>
FlashDspProgramSource::loadProgram()
{
    const esp_partition_t* part = dspPartition();
    if (part == nullptr) {
        ESP_LOGW(kTag, "dsp partition missing");
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }

    std::vector<std::uint8_t> buffer(part->size);
    if (esp_partition_read(part, 0, buffer.data(), part->size) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    if (partitionLooksEmpty(buffer)) {
        return std::unexpected(core::DspProgramError::Empty);
    }

    return core::parseDspProgramBlob(buffer);
}

std::expected<void, core::DspProgramError>
FlashDspProgramSource::storeBlob(std::span<const std::uint8_t> blob)
{
    const esp_partition_t* part = dspPartition();
    if (part == nullptr || blob.size() > part->size) {
        return std::unexpected(core::DspProgramError::FlashWriteFailed);
    }

    if (esp_partition_erase_range(part, 0, part->size) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashWriteFailed);
    }
    if (esp_partition_write(part, 0, blob.data(), blob.size()) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashWriteFailed);
    }

    ESP_LOGI(kTag, "stored %u byte DSP program blob", static_cast<unsigned>(blob.size()));
    return {};
}

} // namespace adau1701
