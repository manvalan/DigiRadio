/**
 * @file    NvsSecureStore.cpp
 * @brief   NvsSecureStore implementation.
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
 * @date    2026-07-06
 */

#include "secure_store/NvsSecureStore.hpp"

#include "nvs.h"
#include "nvs_flash.h"

#include <string>
#include <vector>

namespace secure_store {

namespace {
constexpr char kNamespace[] = "digiradio";
constexpr char kSsidKey[] = "wifi_ssid";
constexpr char kPasswordKey[] = "wifi_pwd";
constexpr char kStationListKey[] = "station_list";
constexpr char kLastPresetKey[] = "last_preset";
} // namespace

bool NvsSecureStore::hasWifiCredentials() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    std::size_t ssidLen = 0;
    const esp_err_t ssidErr =
        nvs_get_str(handle, kSsidKey, nullptr, &ssidLen);
    nvs_close(handle);

    return ssidErr == ESP_OK && ssidLen > 1;
}

std::expected<void, core::StoreError>
NvsSecureStore::saveWifiCredentials(const core::WifiCredentials& creds)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    const std::string ssid(creds.ssid().value());
    std::string password;
    creds.password().usePlaintext(
        [&](std::string_view pwd) { password.assign(pwd); });

    esp_err_t err = nvs_set_str(handle, kSsidKey, ssid.c_str());
    if (err == ESP_OK) {
        err = nvs_set_str(handle, kPasswordKey, password.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    for (char& ch : password) {
        ch = '\0';
    }

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<core::WifiCredentials, core::StoreError>
NvsSecureStore::loadWifiCredentials() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }

    std::size_t ssidLen = 0;
    if (nvs_get_str(handle, kSsidKey, nullptr, &ssidLen) != ESP_OK
        || ssidLen == 0) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::NotFound);
    }

    std::vector<char> ssidBuf(ssidLen);
    std::size_t pwdLen = 0;
    if (nvs_get_str(handle, kSsidKey, ssidBuf.data(), &ssidLen) != ESP_OK) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::IoFailed);
    }

    if (nvs_get_str(handle, kPasswordKey, nullptr, &pwdLen) != ESP_OK) {
        pwdLen = 0;
    }

    std::string password;
    if (pwdLen > 0) {
        std::vector<char> pwdBuf(pwdLen);
        if (nvs_get_str(handle, kPasswordKey, pwdBuf.data(), &pwdLen) != ESP_OK) {
            nvs_close(handle);
            return std::unexpected(core::StoreError::IoFailed);
        }
        password.assign(pwdBuf.data());
    }

    nvs_close(handle);

    const std::string_view ssidView(ssidBuf.data());
    if (!core::WifiSsid::isValid(ssidView)) {
        return std::unexpected(core::StoreError::InvalidData);
    }

    return core::WifiCredentials(core::WifiSsid(ssidView),
                                 core::Secret(std::move(password)));
}

std::expected<void, core::StoreError> NvsSecureStore::clearWifiCredentials()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_erase_key(handle, kSsidKey);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_erase_key(handle, kPasswordKey);
    }
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

bool NvsSecureStore::hasStationList() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    std::size_t len = 0;
    const esp_err_t err =
        nvs_get_str(handle, kStationListKey, nullptr, &len);
    nvs_close(handle);

    return err == ESP_OK && len > 1;
}

std::expected<void, core::StoreError>
NvsSecureStore::saveStationListJson(std::string_view json)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    const std::string payload(json);
    esp_err_t err = nvs_set_str(handle, kStationListKey, payload.c_str());
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<std::string, core::StoreError>
NvsSecureStore::loadStationListJson() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }

    std::size_t len = 0;
    if (nvs_get_str(handle, kStationListKey, nullptr, &len) != ESP_OK
        || len == 0) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::NotFound);
    }

    std::vector<char> buffer(len);
    if (nvs_get_str(handle, kStationListKey, buffer.data(), &len) != ESP_OK) {
        nvs_close(handle);
        return std::unexpected(core::StoreError::IoFailed);
    }
    nvs_close(handle);

    return std::string(buffer.data());
}

std::expected<void, core::StoreError> NvsSecureStore::clearStationList()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_erase_key(handle, kStationListKey);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

bool NvsSecureStore::hasLastPresetIndex() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    std::uint8_t value = 0U;
    const esp_err_t err = nvs_get_u8(handle, kLastPresetKey, &value);
    nvs_close(handle);

    return err == ESP_OK;
}

std::expected<void, core::StoreError>
NvsSecureStore::saveLastPresetIndex(std::uint8_t index)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_set_u8(handle, kLastPresetKey, index);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

std::expected<std::uint8_t, core::StoreError>
NvsSecureStore::loadLastPresetIndex() const
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }

    std::uint8_t value = 0U;
    const esp_err_t err = nvs_get_u8(handle, kLastPresetKey, &value);
    nvs_close(handle);

    if (err != ESP_OK) {
        return std::unexpected(core::StoreError::NotFound);
    }
    return value;
}

std::expected<void, core::StoreError> NvsSecureStore::clearLastPresetIndex()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        return std::unexpected(core::StoreError::IoFailed);
    }

    esp_err_t err = nvs_erase_key(handle, kLastPresetKey);
    if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

} // namespace secure_store
