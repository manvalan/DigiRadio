/**
 * @file    NetBootstrap.hpp
 * @brief   Owns network resources for setup or STA mode for app lifetime.
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

#include "core/CompanionChipStatus.hpp"
#include "core/DeviceIdentity.hpp"
#include "core/ISecureStore.hpp"
#include "net/NetError.hpp"
#include "net/NetState.hpp"
#include "net/SetupWebServer.hpp"
#include "net/SoftApHost.hpp"
#include "net/StaClient.hpp"

#include <expected>
#include <optional>

namespace audio {
class AudioService;
} // namespace audio

namespace bluetooth {
class BluetoothService;
} // namespace bluetooth

namespace station {
class StationService;
} // namespace station

namespace integration {
class IntegrationService;
} // namespace integration

namespace ota {
class OtaService;
} // namespace ota

namespace tuner {
class TunerService;
} // namespace tuner

namespace net {

/**
 * @brief    NetBootstrap — brings up SoftAP or STA plus the HTTP server.
 *
 * @dname    NetBootstrap
 * @return   n/a (type)
 * @pubstate Owns optional softAp_, optional sta_, and webServer_. Must
 *           outlive app_main; keep one instance alive for process lifetime.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class NetBootstrap {
public:
    /**
     * @brief    start — init platform and join stored Wi-Fi or open SoftAP.
     *
     * @dname    start
     * @param    store           Secure store consulted for saved STA credentials.
     * @param    tuner           Tuner service exposed by the HTTP API.
     * @param    audio           Audio service exposed by the HTTP API.
     * @param    bluetooth       Bluetooth pairing service for REST routes.
     * @param    stations        Station preset service for REST routes.
     * @param    integration     Application orchestration for preset recall.
     * @param    ota             Firmware OTA service for POST /api/system/ota.
     * @param    companionChips  Boot flags exposed on GET /api/health.
     * @param    deviceIdentity  EEPROM-derived SSID, hostname, and serial.
     * @return   NetBootstrap on success, or a NetError.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static std::expected<NetBootstrap, NetError>
    start(core::ISecureStore& store, tuner::TunerService& tuner,
          audio::AudioService& audio, bluetooth::BluetoothService& bluetooth,
          station::StationService& stations,
          integration::IntegrationService& integration,
          ota::OtaService& ota,
          core::CompanionChipStatus companionChips,
          const core::DeviceIdentity& deviceIdentity);

    NetBootstrap(const NetBootstrap&) = delete;
    NetBootstrap& operator=(const NetBootstrap&) = delete;

    /**
     * @brief    NetBootstrap — move-construct from a started bootstrap.
     *
     * @dname    NetBootstrap
     * @param    other  Source instance; left empty after the move.
     * @pubstate transfers softAp_, sta_, and webServer_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    NetBootstrap(NetBootstrap&& other) noexcept = default;

    /**
     * @brief    operator= — move-assign from a started bootstrap.
     *
     * @dname    operator=
     * @param    other  Source instance; left empty after the move.
     * @return   Reference to this instance.
     * @pubstate transfers softAp_, sta_, and webServer_ from other.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    NetBootstrap& operator=(NetBootstrap&& other) noexcept = default;

    /**
     * @brief    ~NetBootstrap — tear down network subsystems.
     *
     * @dname    ~NetBootstrap
     * @pubstate destroys softAp_, sta_, and webServer_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~NetBootstrap() = default;

    /**
     * @brief    state — read the active network phase.
     *
     * @dname    state
     * @return   Current NetState.
     * @pubstate reads state_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] NetState state() const noexcept;

private:
    NetBootstrap(std::optional<SoftApHost> softAp,
                 std::optional<StaClient> sta,
                 SetupWebServer webServer,
                 NetState state);

    std::optional<SoftApHost> softAp_;
    std::optional<StaClient> sta_;
    SetupWebServer webServer_;
    NetState state_;
};

} // namespace net
