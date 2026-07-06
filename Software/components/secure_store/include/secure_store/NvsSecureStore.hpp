/**
 * @file    NvsSecureStore.hpp
 * @brief   NVS-backed ISecureStore for Wi-Fi credentials (Slice 2).
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
 * Production builds should enable NVS encryption (nvs_keys partition in
 * partitions.csv) per ESP-IDF security docs; this slice uses plain NVS
 * for development bring-up.
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/ISecureStore.hpp"

namespace secure_store {

/**
 * @brief    NvsSecureStore — persists credentials in an NVS namespace.
 *
 * @dname    NvsSecureStore
 * @return   n/a (type)
 * @pubstate Opens namespace digiradio on each operation. Passwords are
 *           stored as NVS strings and never logged.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class NvsSecureStore final : public core::ISecureStore {
public:
    /**
     * @brief    NvsSecureStore — default-construct the store accessor.
     *
     * @dname    NvsSecureStore
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    NvsSecureStore() = default;

    /**
     * @brief    hasWifiCredentials — check whether STA creds are stored.
     *
     * @dname    hasWifiCredentials
     * @return   true when SSID key exists in NVS.
     * @pubstate reads NVS namespace digiradio.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] bool hasWifiCredentials() const override;

    /**
     * @brief    saveWifiCredentials — persist validated STA credentials.
     *
     * @dname    saveWifiCredentials
     * @param    creds  Validated credentials to persist.
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate writes NVS keys wifi_ssid and wifi_pwd.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError>
    saveWifiCredentials(const core::WifiCredentials& creds) override;

    /**
     * @brief    loadWifiCredentials — read stored STA credentials.
     *
     * @dname    loadWifiCredentials
     * @return   WifiCredentials on success, or a StoreError.
     * @pubstate reads NVS namespace digiradio.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<core::WifiCredentials, core::StoreError>
    loadWifiCredentials() const override;

    /**
     * @brief    clearWifiCredentials — erase stored STA credentials.
     *
     * @dname    clearWifiCredentials
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate erases wifi_ssid and wifi_pwd from NVS.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError>
    clearWifiCredentials() override;
};

} // namespace secure_store
