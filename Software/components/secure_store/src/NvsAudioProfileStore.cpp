/**
 * @file    NvsAudioProfileStore.cpp
 * @brief   NvsAudioProfileStore implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "secure_store/NvsAudioProfileStore.hpp"

#include "core/AudioProfileJson.hpp"

#include "esp_log.h"
#include "nvs.h"

#include <string>
#include <vector>

namespace secure_store {

namespace {
constexpr char kTag[] = "NvsAudioProfileStore";
constexpr char kNamespace[] = "digiradio";
// NVS key names are capped at 15 chars (NVS_KEY_NAME_MAX_SIZE=16 incl. NUL);
// the previous "audio_profile_json" (18 chars) made every nvs_set_str call
// fail with ESP_ERR_NVS_KEY_TOO_LONG (0x1109), silently -- applyProfile()
// always updated live audio correctly but persistProfile() never actually
// wrote anything, so nothing survived a reboot.
constexpr char kProfileKey[] = "audio_profile";
} // namespace

bool NvsAudioProfileStore::hasProfile() const
{
    nvs_handle_t handle = 0;
    const esp_err_t openErr = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (openErr != ESP_OK) {
        // ESP_ERR_NVS_NOT_INITIALIZED (0x1101) here means this was called
        // before secure_store::initEncryptedStorage() -- see 2026-08-24 fix
        // in main.cpp's app_main() boot order (NVS must init before
        // AudioService::loadAndApply(), which calls this).
        ESP_LOGW(kTag, "hasProfile: nvs_open failed (0x%x)",
                 static_cast<unsigned>(openErr));
        return false;
    }

    std::size_t len = 0;
    const esp_err_t err = nvs_get_str(handle, kProfileKey, nullptr, &len);
    nvs_close(handle);
    return err == ESP_OK && len > 1U;
}

std::expected<void, core::StoreError> NvsAudioProfileStore::saveProfile(
    const core::AudioProfile& profile)
{
    const std::string json = core::serializeAudioProfileJson(profile);

    nvs_handle_t handle = 0;
    esp_err_t openErr = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (openErr != ESP_OK) {
        ESP_LOGW(kTag, "nvs_open failed (0x%x)", static_cast<unsigned>(openErr));
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_set_str(handle, kProfileKey, json.c_str());
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "nvs_set_str failed (0x%x) json_len=%u",
                static_cast<unsigned>(err),
                static_cast<unsigned>(json.size()));
    } else {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "nvs_commit failed (0x%x)",
                    static_cast<unsigned>(err));
        }
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<core::AudioProfile, core::StoreError>
NvsAudioProfileStore::loadProfile() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }

    std::size_t len = 0;
    if (nvs_get_str(handle, kProfileKey, nullptr, &len) != ESP_OK || len == 0U) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::NotFound);
    }

    std::vector<char> buf(len);
    if (nvs_get_str(handle, kProfileKey, buf.data(), &len) != ESP_OK) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::IoFailed);
    }
    nvs_close(handle);

    if (auto parsed = core::parseAudioProfileJson(buf.data()); parsed) {
        return *parsed;
    }
    return std::unexpected(core::StoreError::InvalidData);
}

std::expected<void, core::StoreError> NvsAudioProfileStore::clearProfile()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_erase_key(handle, kProfileKey);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

} // namespace secure_store
