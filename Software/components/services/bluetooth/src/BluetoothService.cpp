/**
 * @file    BluetoothService.cpp
 * @brief   BluetoothService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "bluetooth/BluetoothService.hpp"

namespace bluetooth {

BluetoothService::BluetoothService(bt1035::Bt1035Driver& driver)
    : driver_(driver)
    , pairingActive_(false)
{
}

std::expected<core::BluetoothStatus, bt1035::Bt1035Error>
BluetoothService::refreshStatus()
{
    core::BluetoothStatus status{
        .booted = driver_.isBooted(),
        .pairing = pairingActive_,
        .a2dpState = core::Bt1035A2dpState::Standby,
        .deviceName = {},
        .autoReconnect = 0U,
    };

    if (!status.booted) {
        return status;
    }

    if (auto name = driver_.queryDeviceName(); name) {
        status.deviceName = std::move(*name);
    } else {
        return std::unexpected(name.error());
    }

    if (auto reconnect = driver_.queryAutoReconnect(); reconnect) {
        status.autoReconnect = *reconnect;
    } else {
        return std::unexpected(reconnect.error());
    }

    auto a2dp = driver_.queryA2dpState();
    if (!a2dp) {
        return std::unexpected(a2dp.error());
    }
    status.a2dpState = *a2dp;
    return status;
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::startPairing()
{
    if (auto result = driver_.enterPairingMode(); !result) {
        return result;
    }
    pairingActive_ = true;
    return {};
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::stopPairing()
{
    if (auto result = driver_.leavePairingMode(); !result) {
        return result;
    }
    pairingActive_ = false;
    return {};
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::disconnect()
{
    return driver_.disconnectA2dp();
}

std::expected<std::vector<core::Bt1035PairedDevice>, bt1035::Bt1035Error>
BluetoothService::listPaired()
{
    return driver_.queryPairedList();
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::setAutoReconnect(
    std::uint8_t times)
{
    return driver_.setAutoReconnect(times);
}

} // namespace bluetooth
