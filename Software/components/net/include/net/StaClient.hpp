/**
 * @file    StaClient.hpp
 * @brief   RAII helper that joins a Wi-Fi network in STA mode.
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

#include "core/WifiCredentials.hpp"
#include "net/NetError.hpp"

#include <expected>
#include <string_view>

namespace net {

/**
 * @brief    StaClient — connects the ESP32 to a stored Wi-Fi network.
 *
 * @dname    StaClient
 * @return   n/a (type)
 * @pubstate Owns connected_ (whether STA link + IP are up). Assumes
 *           esp_wifi_init() was already called by NetBootstrap.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class StaClient {
public:
    /**
     * @brief    StaClient — construct an unconnected STA client.
     *
     * @dname    StaClient
     * @pubstate clears connected_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    StaClient();

    /**
     * @brief    ~StaClient — disconnect STA if connected.
     *
     * @dname    ~StaClient
     * @pubstate stops Wi-Fi when connected_ is true.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~StaClient();

    StaClient(const StaClient&) = delete;
    StaClient& operator=(const StaClient&) = delete;

    /**
     * @brief    StaClient — move-construct, transferring connection state.
     *
     * @dname    StaClient
     * @param    other  Source client; left disconnected after the move.
     * @pubstate transfers connected_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    StaClient(StaClient&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring connection state.
     *
     * @dname    operator=
     * @param    other  Source client; left disconnected after the move.
     * @return   Reference to this instance.
     * @pubstate transfers connected_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    StaClient& operator=(StaClient&& other) noexcept;

    /**
     * @brief    connect — join the network described by creds.
     *
     * @dname    connect
     * @param    creds     Validated domain credentials from ISecureStore.
     * @param    hostname  STA hostname / mDNS label (no .local suffix).
     * @return   Ok on success, or NetError::StaConnectTimeout /
     *           NetError::StaConnectFailed.
     * @pubstate writes connected_ on success; uses creds via Secret.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, NetError>
    connect(const core::WifiCredentials& creds,
            std::string_view hostname = {});

private:
    bool connected_;
};

} // namespace net
