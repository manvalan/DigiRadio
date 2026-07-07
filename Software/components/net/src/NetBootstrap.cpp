/**
 * @file    NetBootstrap.cpp
 * @brief   NetBootstrap implementation.
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

#include "net/NetBootstrap.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "audio/AudioService.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "secure_store/NvsPlatformInit.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"

namespace net {

namespace {
constexpr char kTag[] = "NetBootstrap";

/**
 * @brief    initPlatform — one-time NVS and TCP/IP stack bring-up.
 *
 * @dname    initPlatform
 * @return   Ok on success, or a NetError describing the failure.
 * @pubstate initialises encrypted NVS, esp_netif, and the default event loop.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<void, NetError> initPlatform()
{
    const auto nvsResult = secure_store::initEncryptedStorage();
    if (!nvsResult) {
        ESP_LOGE(kTag, "encrypted NVS init failed");
        return std::unexpected(NetError::NvsInitFailed);
    }

    if (esp_netif_init() != ESP_OK) {
        ESP_LOGE(kTag, "esp_netif_init failed");
        return std::unexpected(NetError::NetifInitFailed);
    }

    if (esp_event_loop_create_default() != ESP_OK) {
        ESP_LOGE(kTag, "esp_event_loop_create_default failed");
        return std::unexpected(NetError::EventLoopFailed);
    }

    return {};
}

/**
 * @brief    initWifiStack — initialise the Wi-Fi driver once.
 *
 * @dname    initWifiStack
 * @return   Ok on success, or NetError::WifiInitFailed.
 * @pubstate calls esp_wifi_init.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<void, NetError> initWifiStack()
{
    wifi_init_config_t initCfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&initCfg) != ESP_OK) {
        ESP_LOGE(kTag, "esp_wifi_init failed");
        return std::unexpected(NetError::WifiInitFailed);
    }
    return {};
}

/**
 * @brief    startSetupMode — SoftAP plus HTTP for first-time provisioning.
 *
 * @dname    startSetupMode
 * @param    store  Store passed through to the web server routes.
 * @return   NetBootstrap in SoftApSetup, or a NetError.
 * @pubstate creates default Wi-Fi AP netif and starts SoftAP.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<NetBootstrap, NetError>
startSetupMode(core::ISecureStore& store, tuner::TunerService& tuner,
               audio::AudioService& audio,
               bluetooth::BluetoothService& bluetooth,
               station::StationService& stations,
               integration::IntegrationService& integration,
               ota::OtaService& ota,
               core::CompanionChipStatus companionChips,
               const core::DeviceIdentity& deviceIdentity)
{
    esp_netif_create_default_wifi_ap();

    SoftApHost softAp(SoftApConfig::forSsid(deviceIdentity.softApSsid()));
    if (auto apResult = softAp.start(); !apResult) {
        return std::unexpected(apResult.error());
    }

    SetupWebServer webServer;
    if (auto webResult =
            webServer.start(store, NetState::SoftApSetup, tuner, audio,
                            bluetooth, stations, integration, ota,
                            companionChips, deviceIdentity);
        !webResult) {
        return std::unexpected(webResult.error());
    }

    ESP_LOGI(kTag, "setup mode ready — SSID %.*s",
             static_cast<int>(deviceIdentity.softApSsid().size()),
             deviceIdentity.softApSsid().data());
    return NetBootstrap(std::move(softAp), std::nullopt, std::move(webServer),
                        NetState::SoftApSetup);
}

/**
 * @brief    startStaMode — join stored Wi-Fi and serve HTTP on STA.
 *
 * @dname    startStaMode
 * @param    store  Store supplying validated STA credentials.
 * @return   NetBootstrap in StaConnected, or a NetError.
 * @pubstate creates default Wi-Fi STA netif and connects.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] std::expected<NetBootstrap, NetError>
startStaMode(core::ISecureStore& store, tuner::TunerService& tuner,
             audio::AudioService& audio,
             bluetooth::BluetoothService& bluetooth,
             station::StationService& stations,
             integration::IntegrationService& integration,
             ota::OtaService& ota,
             core::CompanionChipStatus companionChips,
             const core::DeviceIdentity& deviceIdentity)
{
    auto credsResult = store.loadWifiCredentials();
    if (!credsResult) {
        ESP_LOGE(kTag, "stored credentials missing");
        return std::unexpected(NetError::CredentialsNotFound);
    }

    esp_netif_create_default_wifi_sta();

    StaClient sta;
    if (auto staResult =
            sta.connect(credsResult.value(), deviceIdentity.hostname());
        !staResult) {
        return std::unexpected(staResult.error());
    }

    SetupWebServer webServer;
    if (auto webResult =
            webServer.start(store, NetState::StaConnected, tuner, audio,
                            bluetooth, stations, integration, ota,
                            companionChips, deviceIdentity);
        !webResult) {
        return std::unexpected(webResult.error());
    }

    ESP_LOGI(kTag, "STA mode ready — hostname %.*s.local",
             static_cast<int>(deviceIdentity.hostname().size()),
             deviceIdentity.hostname().data());
    return NetBootstrap(std::nullopt, std::move(sta), std::move(webServer),
                        NetState::StaConnected);
}

} // namespace

std::expected<NetBootstrap, NetError>
NetBootstrap::start(core::ISecureStore& store, tuner::TunerService& tuner,
                    audio::AudioService& audio,
                    bluetooth::BluetoothService& bluetooth,
                    station::StationService& stations,
                    integration::IntegrationService& integration,
                    ota::OtaService& ota,
                    core::CompanionChipStatus companionChips,
                    const core::DeviceIdentity& deviceIdentity)
{
    if (auto platform = initPlatform(); !platform) {
        return std::unexpected(platform.error());
    }

    if (auto wifi = initWifiStack(); !wifi) {
        return std::unexpected(wifi.error());
    }

    if (store.hasWifiCredentials()) {
        auto staResult = startStaMode(store, tuner, audio, bluetooth, stations,
                                      integration, ota, companionChips,
                                      deviceIdentity);
        if (staResult) {
            return staResult;
        }
        ESP_LOGW(kTag, "STA join failed — falling back to setup SoftAP");
    }

    return startSetupMode(store, tuner, audio, bluetooth, stations, integration,
                          ota, companionChips, deviceIdentity);
}

NetBootstrap::NetBootstrap(std::optional<SoftApHost> softAp,
                           std::optional<StaClient> sta,
                           SetupWebServer webServer,
                           NetState state)
    : softAp_(std::move(softAp))
    , sta_(std::move(sta))
    , webServer_(std::move(webServer))
    , state_(state)
{
}

NetState NetBootstrap::state() const noexcept
{
    return state_;
}

} // namespace net
