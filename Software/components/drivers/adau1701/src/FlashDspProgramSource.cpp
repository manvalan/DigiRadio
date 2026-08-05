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

    // Partizione non vuota: alloca il backing store con new(nothrow), cosi'
    // un OOM ritorna nullptr invece di abortire (nessuna eccezione).
    auto* raw = new (std::nothrow) std::uint8_t[part->size];
    if (raw == nullptr) {
        ESP_LOGE(kTag, "dsp buffer alloc failed (%u byte)",
                 static_cast<unsigned>(part->size));
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }
    std::unique_ptr<std::uint8_t[]> guard(raw);

    if (esp_partition_read(part, 0, raw, part->size) != ESP_OK) {
        return std::unexpected(core::DspProgramError::FlashReadFailed);
    }

    std::span<const std::uint8_t> view(raw, part->size);
    if (partitionLooksEmpty(view)) {
        return std::unexpected(core::DspProgramError::Empty);
    }

    return core::parseDspProgramBlob(view);
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
