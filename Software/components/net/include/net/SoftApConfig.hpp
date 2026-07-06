/**
 * @file    SoftApConfig.hpp
 * @brief   Configuration value type for the setup SoftAP.
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

#include <cstdint>
#include <string_view>

namespace net {

/**
 * @brief    SoftApConfig — immutable SoftAP parameters for first-time setup.
 *
 * @dname    SoftApConfig
 * @param    ssid             Broadcast SSID (non-empty).
 * @param    channel          Wi-Fi channel (1–13).
 * @param    maxConnections   Maximum associated stations.
 * @return   n/a (type)
 * @pubstate Owns no handles; pure configuration snapshot.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class SoftApConfig {
public:
    /**
     * @brief    setupDefault — factory for the Slice 1 setup SoftAP.
     *
     * @dname    setupDefault
     * @return   SoftApConfig with SSID DigiRadio-setup.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static SoftApConfig setupDefault();

    /**
     * @brief    ssid — read the broadcast SSID.
     *
     * @dname    ssid
     * @return   SSID string view.
     * @pubstate reads ssid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::string_view ssid() const noexcept;

    /**
     * @brief    channel — read the Wi-Fi channel.
     *
     * @dname    channel
     * @return   Channel number.
     * @pubstate reads channel_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint8_t channel() const noexcept;

    /**
     * @brief    maxConnections — read the station limit.
     *
     * @dname    maxConnections
     * @return   Maximum associated clients.
     * @pubstate reads maxConnections_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::uint8_t maxConnections() const noexcept;

private:
    SoftApConfig(std::string_view ssid,
                 std::uint8_t channel,
                 std::uint8_t maxConnections);

    std::string_view ssid_;
    std::uint8_t channel_;
    std::uint8_t maxConnections_;
};

} // namespace net
