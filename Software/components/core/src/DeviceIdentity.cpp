/**
 * @file    DeviceIdentity.cpp
 * @brief   DeviceIdentity implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "core/DeviceIdentity.hpp"

namespace core {

namespace {

constexpr std::string_view kSerialUnknown = "unknown";
constexpr std::string_view kSoftApFallback = "DigiRadio-setup";
constexpr std::string_view kBluetoothFallback = "DigiRadio";
constexpr std::string_view kHostnameFallback = "digiradio";

[[nodiscard]] std::string prefixed(std::string_view prefix,
                                   std::string_view suffix)
{
    return std::string(prefix) + std::string(suffix);
}

} // namespace

DeviceIdentity DeviceIdentity::unknown() noexcept
{
    return DeviceIdentity(std::nullopt,
                          std::string(kSerialUnknown),
                          std::string(kSoftApFallback),
                          std::string(kBluetoothFallback),
                          std::string(kHostnameFallback));
}

DeviceIdentity DeviceIdentity::fromEui48(Eui48 eui)
{
    const std::string suffix = eui.shortSuffix();
    return DeviceIdentity(eui,
                          eui.serialNumber(),
                          prefixed("DigiRadio-", suffix),
                          prefixed("DigiRadio-", suffix),
                          prefixed("digiradio-", suffix));
}

bool DeviceIdentity::isKnown() const noexcept
{
    return eui_.has_value();
}

std::string_view DeviceIdentity::serialNumber() const noexcept
{
    return serialNumber_;
}

std::string_view DeviceIdentity::softApSsid() const noexcept
{
    return softApSsid_;
}

std::string_view DeviceIdentity::bluetoothName() const noexcept
{
    return bluetoothName_;
}

std::string_view DeviceIdentity::hostname() const noexcept
{
    return hostname_;
}

const std::optional<Eui48>& DeviceIdentity::eui48() const noexcept
{
    return eui_;
}

DeviceIdentity::DeviceIdentity(std::optional<Eui48> eui,
                               std::string serialNumber,
                               std::string softApSsid,
                               std::string bluetoothName,
                               std::string hostname)
    : eui_(std::move(eui))
    , serialNumber_(std::move(serialNumber))
    , softApSsid_(std::move(softApSsid))
    , bluetoothName_(std::move(bluetoothName))
    , hostname_(std::move(hostname))
{
}

} // namespace core
