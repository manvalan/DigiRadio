/**
 * @file    WifiSsid.hpp
 * @brief   Strong type for a Wi-Fi network name (802.11 SSID).
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

#include <cstddef>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    WifiSsid — validated Wi-Fi SSID (1–32 bytes).
 *
 * @dname    WifiSsid
 * @return   n/a (type)
 * @pubstate Owns ssid_ (immutable after construction).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class WifiSsid {
public:
    /** Maximum SSID length per 802.11. */
    static constexpr std::size_t kMaxLength = 32;

    /**
     * @brief    tryFrom — parse and validate an SSID at the boundary.
     *
     * @dname    tryFrom
     * @param    raw  Untrusted SSID string from network input.
     * @return   WifiSsid on success, or empty optional if invalid.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static bool isValid(std::string_view raw) noexcept;

    /**
     * @brief    WifiSsid — construct from an already-validated SSID.
     *
     * @dname    WifiSsid
     * @param    raw  Non-empty SSID up to 32 bytes.
     * @pubstate writes ssid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    explicit WifiSsid(std::string_view raw);

    /**
     * @brief    value — read the SSID string.
     *
     * @dname    value
     * @return   Stored SSID as a string view.
     * @pubstate reads ssid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::string_view value() const noexcept;

private:
    std::string ssid_;
};

} // namespace core
