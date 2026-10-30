/**
 * @file    NvsAudioPresetStore.cpp
 * @brief   NVS-backed IAudioPresetStore (primary copy of the preset bank).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "secure_store/NvsAudioPresetStore.hpp"

#include "core/AudioPresetCodec.hpp"

#include "esp_log.h"
#include "nvs.h"

#include <vector>

namespace secure_store {

namespace {
constexpr char kTag[] = "NvsAudioPresetStore";
constexpr char kNamespace[] = "digiradio";
constexpr char kPresetsKey[] = "audio_presets"; // <= 15 chars (NVS limit)
} // namespace

std::expected<core::AudioPresetBank, core::StoreError>
NvsAudioPresetStore::loadBank() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }

    std::size_t len = 0;
    if (nvs_get_blob(handle, kPresetsKey, nullptr, &len) != ESP_OK
        || len == 0U) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::NotFound);
    }

    std::vector<std::uint8_t> blob(len);
    const esp_err_t err = nvs_get_blob(handle, kPresetsKey, blob.data(), &len);
    nvs_close(handle);
    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return core::decodePresetBankBlob(blob);
}

std::expected<void, core::StoreError> NvsAudioPresetStore::saveBank(
    const core::AudioPresetBank& bank)
{
    const std::vector<std::uint8_t> blob = core::encodePresetBankBlob(bank);

    nvs_handle_t handle = 0;
    const esp_err_t openErr = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (openErr != ESP_OK) {
        ESP_LOGW(kTag, "nvs_open failed (0x%x)", static_cast<unsigned>(openErr));
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_set_blob(handle, kPresetsKey, blob.data(), blob.size());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "save failed (0x%x) len=%u", static_cast<unsigned>(err),
                 static_cast<unsigned>(blob.size()));
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

} // namespace secure_store
