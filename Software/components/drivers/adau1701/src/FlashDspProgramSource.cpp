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

#include <cstdlib>
#include <memory>

namespace adau1701 {

namespace {

constexpr char kTag[] = "FlashDsp";
constexpr esp_partition_subtype_t kDspPartitionSubtype =
    static_cast<esp_partition_subtype_t>(0x40U);

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

    // Rileva la partizione vuota leggendo solo un piccolo header, PRIMA di
    // allocare l'intera partizione (256KB). Con eccezioni C++ disabilitate,
    // allocare un vector cosi' grande su OOM chiama abort(). La partizione
    // vergine e' tutta 0xFF.
    constexpr std::size_t kProbeSize = 64U;
    std::uint8_t probe[kProbeSize] = {};
    if (esp_partition_read(part, 0, probe, kProbeSize) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    bool allErased = true;
    for (std::size_t i = 0; i < kProbeSize; ++i) {
        if (probe[i] != 0xFFU) { allErased = false; break; }
    }
    if (allErased) {
        ESP_LOGW(kTag, "dsp partition empty (erased) - using fallback");
        return std::unexpected(core::DspProgramError::Empty);
    }

    // Determine exact blob size by scanning DRAD write records before
    // allocating — the full partition (256 KB) exceeds the available heap
    // on ESP32-S3, and new(nothrow) still invokes __cxa_allocate_exception
    // when exceptions are disabled, causing abort().

    // Read DRAD header (12 bytes) to get write_count.
    constexpr std::size_t kHdrSize = 12U;
    std::uint8_t hdr[kHdrSize] = {};
    if (esp_partition_read(part, 0, hdr, kHdrSize) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    // Quick magic/version check (full validation done by parseDspProgramBlob).
    if (hdr[0] != 'D' || hdr[1] != 'R' || hdr[2] != 'A' || hdr[3] != 'D'
        || static_cast<std::uint16_t>(hdr[4] | (hdr[5] << 8)) != 1U) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    const auto writeCount =
        static_cast<std::uint16_t>(hdr[6] | (hdr[7] << 8));
    if (writeCount == 0U || writeCount > 32U) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }

    // Scan write record headers (4 bytes each) to compute total payload size.
    std::size_t payloadSize = 0U;
    for (std::uint16_t i = 0U; i < writeCount; ++i) {
        std::uint8_t rec[4] = {};
        if (esp_partition_read(part, kHdrSize + payloadSize, rec, 4U)
            != ESP_OK) {
            return std::unexpected(core::DspProgramError::FlashReadFailed);
        }
        const auto dataLen =
            static_cast<std::uint16_t>(rec[2] | (rec[3] << 8));
        if (dataLen == 0U || dataLen > 16384U) {
            return std::unexpected(core::DspProgramError::FlashReadFailed);
        }
        payloadSize += 4U + dataLen;
    }

    const std::size_t totalSize = kHdrSize + payloadSize;

    // Use malloc — avoids C++ exception machinery entirely (no nothrow workaround).
    auto* raw = static_cast<std::uint8_t*>(::malloc(totalSize));
    if (raw == nullptr) {
        ESP_LOGE(kTag, "dsp buffer alloc failed (%u bytes)",
                 static_cast<unsigned>(totalSize));
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    std::unique_ptr<std::uint8_t, decltype(&::free)> guard(raw, ::free);

    if (esp_partition_read(part, 0, raw, totalSize) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }

    return core::parseDspProgramBlob({raw, totalSize});
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
