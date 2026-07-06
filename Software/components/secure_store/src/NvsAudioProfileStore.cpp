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

#include "nvs.h"

#include <string>
#include <vector>

namespace secure_store {

namespace {
constexpr char kNamespace[] = "digiradio";
constexpr char kProfileKey[] = "audio_profile_json";
} // namespace

bool NvsAudioProfileStore::hasProfile() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
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
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_set_str(handle, kProfileKey, json.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
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
