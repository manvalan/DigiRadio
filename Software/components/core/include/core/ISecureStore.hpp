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

#include <expected>

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
};

} // namespace core
