/**
 * @file    ISecureStore.hpp
 * @brief   Abstract secure persistence for credentials and lists.
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
#pragma once

#include "core/StoreError.hpp"
#include "core/WifiCredentials.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    ISecureStore — persistence boundary for secrets at rest.
 *
 * @dname    ISecureStore
 * @return   n/a (type)
 * @pubstate Implementations own NVS/flash handles in the shell. The pure
 *           core and host tests use fakes; never touch real keys here.
 *
 * Slice 2 stores Wi-Fi credentials. Station list and user credentials
 * arrive in later slices on the same interface.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class ISecureStore {
public:
    /**
     * @brief    ~ISecureStore — virtual destructor for interface.
     *
     * @dname    ~ISecureStore
     * @pubstate n/a
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    virtual ~ISecureStore() = default;

    /**
     * @brief    hasWifiCredentials — check whether STA creds are stored.
     *
     * @dname    hasWifiCredentials
     * @return   true when loadWifiCredentials would succeed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual bool hasWifiCredentials() const = 0;

    /**
     * @brief    saveWifiCredentials — persist validated STA credentials.
     *
     * @dname    saveWifiCredentials
     * @param    creds  Domain credentials; password stays wrapped in Secret.
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate writes backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    saveWifiCredentials(const WifiCredentials& creds) = 0;

    /**
     * @brief    loadWifiCredentials — read stored STA credentials.
     *
     * @dname    loadWifiCredentials
     * @return   WifiCredentials on success, or StoreError::NotFound /
     *           StoreError::InvalidData / StoreError::IoFailed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<WifiCredentials, StoreError>
    loadWifiCredentials() const = 0;

    /**
     * @brief    clearWifiCredentials — erase stored STA credentials.
     *
     * @dname    clearWifiCredentials
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate clears backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    clearWifiCredentials() = 0;

    /**
     * @brief    hasStationList — check whether presets are stored.
     *
     * @dname    hasStationList
     * @return   true when loadStationListJson would succeed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual bool hasStationList() const = 0;

    /**
     * @brief    saveStationListJson — persist serialised preset list JSON.
     *
     * @dname    saveStationListJson
     * @param    json  Output of core::serializeStationListJson().
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate writes backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    saveStationListJson(std::string_view json) = 0;

    /**
     * @brief    loadStationListJson — read stored preset list JSON.
     *
     * @dname    loadStationListJson
     * @return   JSON blob on success, or StoreError.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<std::string, StoreError>
    loadStationListJson() const = 0;

    /**
     * @brief    clearStationList — erase stored presets.
     *
     * @dname    clearStationList
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate clears backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    clearStationList() = 0;

    /**
     * @brief    hasLastPresetIndex — check whether a last-recalled preset exists.
     *
     * @dname    hasLastPresetIndex
     * @return   true when loadLastPresetIndex would succeed.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual bool hasLastPresetIndex() const = 0;

    /**
     * @brief    saveLastPresetIndex — persist the last recalled preset index.
     *
     * @dname    saveLastPresetIndex
     * @param    index  Zero-based preset list position (0–19).
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate writes backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    saveLastPresetIndex(std::uint8_t index) = 0;

    /**
     * @brief    loadLastPresetIndex — read the last recalled preset index.
     *
     * @dname    loadLastPresetIndex
     * @return   Preset index on success, or StoreError.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<std::uint8_t, StoreError>
    loadLastPresetIndex() const = 0;

    /**
     * @brief    clearLastPresetIndex — erase the last-recalled preset marker.
     *
     * @dname    clearLastPresetIndex
     * @return   Ok on success, or StoreError::IoFailed.
     * @pubstate clears backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] virtual std::expected<void, StoreError>
    clearLastPresetIndex() = 0;
};

} // namespace core
