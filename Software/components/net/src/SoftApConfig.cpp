/**
 * @file    SoftApConfig.cpp
 * @brief   SoftApConfig implementation.
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

#include "net/SoftApConfig.hpp"

namespace net {

namespace {
constexpr std::string_view kSetupSsid = "DigiRadio-setup";
constexpr std::uint8_t kSetupChannel = 1;
constexpr std::uint8_t kSetupMaxConnections = 4;
} // namespace

SoftApConfig SoftApConfig::setupDefault()
{
    return forSsid(kSetupSsid);
}

SoftApConfig SoftApConfig::forSsid(std::string_view ssid)
{
    return SoftApConfig(ssid, kSetupChannel, kSetupMaxConnections);
}

SoftApConfig::SoftApConfig(std::string_view ssid,
                           std::uint8_t channel,
                           std::uint8_t maxConnections)
    : ssid_(ssid)
    , channel_(channel)
    , maxConnections_(maxConnections)
{
}

std::string_view SoftApConfig::ssid() const noexcept
{
    return ssid_;
}

std::uint8_t SoftApConfig::channel() const noexcept
{
    return channel_;
}

std::uint8_t SoftApConfig::maxConnections() const noexcept
{
    return maxConnections_;
}

} // namespace net
