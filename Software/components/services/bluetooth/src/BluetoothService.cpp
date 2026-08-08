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

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace bluetooth {

namespace {
constexpr char kTag[] = "BluetoothSvc";
constexpr int kConnectSettleMs = 30000;
constexpr int kStreamSettleMs = 20000;
constexpr int kAutoReconnectWaitMs = 45000;

[[nodiscard]] bool isA2dpLinked(core::Bt1035A2dpState state) noexcept
{
    using core::Bt1035A2dpState;
    return state == Bt1035A2dpState::Connected
           || state == Bt1035A2dpState::Streaming
           || state == Bt1035A2dpState::Paused;
}

[[nodiscard]] bool isA2dpLinked(bt1035::Bt1035Driver& driver)
{
    if (auto state = driver.queryA2dpState(); state) {
        return isA2dpLinked(*state);
    }
    return false;
}

[[nodiscard]] bool ensureA2dpStreaming(bt1035::Bt1035Driver& driver)
{
    if (auto state = driver.queryA2dpState(); state
        && *state == core::Bt1035A2dpState::Streaming) {
        ESP_LOGI(kTag, "A2DP already streaming");
        return true;
    }
    if (!driver.waitForA2dpStreaming(kStreamSettleMs)) {
        ESP_LOGW(kTag, "A2DP not streaming within %d ms", kStreamSettleMs);
        return false;
    }
    ESP_LOGI(kTag, "A2DP streaming OK");
    return true;
}

void savedSpeakerReconnectTask(void* arg)
{
    auto* service = static_cast<BluetoothService*>(arg);
    ESP_LOGI(kTag, "boot reconnect task started");
    if (auto result = service->reconnectSavedSpeaker(true); !result) {
        ESP_LOGW(kTag, "boot reconnect failed (%d)",
                 static_cast<int>(result.error()));
    } else {
        ESP_LOGI(kTag, "boot reconnect OK");
    }
    vTaskDelete(nullptr);
}
} // namespace

BluetoothService::BluetoothService(bt1035::Bt1035Driver& driver,
                                   core::ISecureStore& store)
    : driver_(driver)
    , store_(store)
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

std::expected<std::vector<core::Bt1035ScannedDevice>, bt1035::Bt1035Error>
BluetoothService::scanNearby(std::uint8_t scanSeconds)
{
    pairingActive_ = false;
    (void)driver_.leavePairingMode();
    (void)driver_.stopScan();
    return driver_.scanNearbyBrEdr(scanSeconds);
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::connectDirect(
    std::string_view mac)
{
    if (auto sent = driver_.connectA2dp(mac); !sent) {
        return sent;
    }
    if (!driver_.waitForA2dpConnected(kConnectSettleMs)) {
        ESP_LOGW(kTag, "A2DP connect sent but link not up in %d ms",
                 kConnectSettleMs);
        return std::unexpected(bt1035::Bt1035Error::AtTimeout);
    }
    if (!ensureA2dpStreaming(driver_)) {
        return std::unexpected(bt1035::Bt1035Error::AtTimeout);
    }
    return {};
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::connectTo(
    std::string_view mac)
{
    pairingActive_ = false;
    return connectDirect(mac);
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::connectTo(
    const core::BluetoothConnectRequest& request)
{
    if (request.mac.empty() || !core::isValidBt1035Mac(request.mac)) {
        return std::unexpected(bt1035::Bt1035Error::UnexpectedResponse);
    }
    pairingActive_ = false;
    if (auto connected = connectDirect(request.mac); !connected) {
        return connected;
    }
    if (request.save) {
        core::BtSpeakerTarget target{
            .mac = request.mac,
            .name = request.name,
        };
        if (auto saved = saveSpeaker(target); !saved) {
            ESP_LOGW(kTag, "connect OK but save speaker failed");
        }
    }
    return {};
}

bool BluetoothService::hasSavedSpeaker() const
{
    return store_.hasBtSpeakerTarget();
}

std::expected<core::BtSpeakerTarget, core::StoreError>
BluetoothService::loadSavedSpeaker() const
{
    return store_.loadBtSpeakerTarget();
}

std::expected<void, core::StoreError> BluetoothService::saveSpeaker(
    const core::BtSpeakerTarget& target)
{
    return store_.saveBtSpeakerTarget(target);
}

std::expected<void, core::StoreError> BluetoothService::clearSavedSpeaker()
{
    return store_.clearBtSpeakerTarget();
}

void BluetoothService::startupReconnect()
{
    if (!hasSavedSpeaker()) {
        ESP_LOGI(kTag, "no saved BT speaker — skip boot reconnect");
        return;
    }
    if (xTaskCreate(savedSpeakerReconnectTask, "bt_reconn", 8192, this, 4,
                    nullptr)
        != pdPASS) {
        ESP_LOGW(kTag, "boot reconnect task create failed");
    }
}

std::expected<void, bt1035::Bt1035Error> BluetoothService::reconnectSavedSpeaker(
    bool scanFallback)
{
    const auto target = loadSavedSpeaker();
    if (!target) {
        return {};
    }

    ESP_LOGI(kTag, "reconnect saved speaker %s (%s)", target->mac.c_str(),
             target->name.empty() ? "(no name)" : target->name.c_str());

    if (isA2dpLinked(driver_)) {
        ESP_LOGI(kTag, "A2DP already linked — ensure streaming");
        if (!ensureA2dpStreaming(driver_)) {
            return std::unexpected(bt1035::Bt1035Error::AtTimeout);
        }
        return {};
    }

    if (auto connected = connectDirect(target->mac); connected) {
        return connected;
    }

    if (isA2dpLinked(driver_)) {
        ESP_LOGI(kTag, "A2DP linked after connect attempt");
        if (!ensureA2dpStreaming(driver_)) {
            return std::unexpected(bt1035::Bt1035Error::AtTimeout);
        }
        return {};
    }

    ESP_LOGI(kTag, "waiting for module auto-reconnect (%d ms)",
             kAutoReconnectWaitMs);
    if (driver_.waitForA2dpConnected(kAutoReconnectWaitMs)) {
        ESP_LOGI(kTag, "A2DP auto-reconnect linked");
        if (!ensureA2dpStreaming(driver_)) {
            return std::unexpected(bt1035::Bt1035Error::AtTimeout);
        }
        return {};
    }

    if (!scanFallback) {
        return std::unexpected(bt1035::Bt1035Error::AtTimeout);
    }

    ESP_LOGW(kTag, "direct A2DPCONN failed — retry connect (no scan)");
    if (auto retry = connectDirect(target->mac); retry) {
        return retry;
    }
    if (isA2dpLinked(driver_)) {
        if (!ensureA2dpStreaming(driver_)) {
            return std::unexpected(bt1035::Bt1035Error::AtTimeout);
        }
        return {};
    }
    return std::unexpected(bt1035::Bt1035Error::AtTimeout);
}

} // namespace bluetooth
