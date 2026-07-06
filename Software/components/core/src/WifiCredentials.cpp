/**
 * @file    WifiCredentials.cpp
 * @brief   WifiCredentials implementation.
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

#include "core/WifiCredentials.hpp"

namespace core {

bool WifiCredentials::isPasswordValid(std::string_view raw) noexcept
{
    if (raw.empty()) {
        return true;
    }
    return raw.size() >= 8 && raw.size() <= kMaxPasswordLength;
}

WifiCredentials::WifiCredentials(WifiSsid ssid, Secret password)
    : ssid_(std::move(ssid))
    , password_(std::move(password))
{
}

const WifiSsid& WifiCredentials::ssid() const noexcept
{
    return ssid_;
}

const Secret& WifiCredentials::password() const noexcept
{
    return password_;
}

} // namespace core
