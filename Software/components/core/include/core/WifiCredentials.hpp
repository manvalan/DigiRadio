/**
 * @file    WifiCredentials.hpp
 * @brief   Domain type for stored Wi-Fi STA credentials.
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

#include "core/Secret.hpp"
#include "core/WifiSsid.hpp"

namespace core {

/**
 * @brief    WifiCredentials — SSID plus protected PSK for STA join.
 *
 * @dname    WifiCredentials
 * @return   n/a (type)
 * @pubstate Owns ssid_ and password_. Password is never exposed as a
 *           loggable string; use Secret::usePlaintext in the shell only.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class WifiCredentials {
public:
    /** Maximum WPA-PSK length accepted at the boundary. */
    static constexpr std::size_t kMaxPasswordLength = 63;

    /**
     * @brief    isPasswordValid — validate a PSK length at the boundary.
     *
     * @dname    isPasswordValid
     * @param    raw  Untrusted password from network input.
     * @return   true when length is 0 (open) or 8–63 (WPA).
     * @pubstate none
     *
     * Open networks use an empty password; WPA-PSK requires 8–63 chars.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static bool isPasswordValid(std::string_view raw) noexcept;

    /**
     * @brief    WifiCredentials — construct from validated domain parts.
     *
     * @dname    WifiCredentials
     * @param    ssid      Validated network name.
     * @param    password  Protected pre-shared key (may be empty for open).
     * @pubstate writes ssid_ and password_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    WifiCredentials(WifiSsid ssid, Secret password);

    /**
     * @brief    ssid — read the network name.
     *
     * @dname    ssid
     * @return   The stored SSID value object.
     * @pubstate reads ssid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const WifiSsid& ssid() const noexcept;

    /**
     * @brief    password — borrow the protected PSK.
     *
     * @dname    password
     * @return   Const reference to the Secret wrapper.
     * @pubstate reads password_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const Secret& password() const noexcept;

private:
    WifiSsid ssid_;
    Secret password_;
};

} // namespace core
