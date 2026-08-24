/**
 * @file    SetupWebServer.hpp
 * @brief   HTTP server for setup UI, health, and Wi-Fi provisioning API.
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

#include "esp_http_server.h"
#include <cstddef>
#include <cstdint>
#include <expected>

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

namespace webradio {
class WebRadioService;
} // namespace webradio

struct httpd_handle;

namespace net {

/**
 * @brief    PhoneStreamSink — plain function pointers over the shared I2S
 *           TX channel, so net/ (protocol-only) never includes I2S driver
 *           headers directly; main/esp32_i2s_sink.cpp supplies them.
 *
 * @dname    PhoneStreamSink
 * @return   n/a (type)
 * @pubstate All members are free functions with process lifetime; no
 *           per-instance state.
 *
 * @author   Michele Bigi
 * @date     2026-08-18
 */
struct PhoneStreamSink {
    /** Claim exclusive use of the sink; false if already held (e.g. by
     *  the web radio stream). */
    bool (*tryAcquire)();
    /** Release a previously acquired claim. */
    void (*release)();
    /** Write one chunk of interleaved 16-bit stereo PCM @ 48 kHz.
     *  @return false on an I2S write error. */
    bool (*writePcm16Stereo)(const std::int16_t* interleaved,
                             std::size_t frameCount);
};

/**
 * @brief    AntennaCalibration — plain function pointers over EEPROM-backed
 *           FM and DAB ANTCAP storage, so net/ never includes eeprom24aa
 *           headers directly; main/ supplies it (HardwareBootstrap owns the
 *           I2C bus and EEPROM handle).
 *
 * @dname    AntennaCalibration
 * @return   n/a (type)
 * @pubstate Free functions with process lifetime; no per-instance state.
 *
 * @author   Michele Bigi
 * @date     2026-08-19
 */
struct AntennaCalibration {
    /** Persist a new FM ANTCAP calibration value to EEPROM.
     *  @return false on an I2C failure. */
    bool (*save)(std::uint8_t antCap);
    /** Persist a new DAB ANTCAP calibration value to EEPROM.
     *  @return false on an I2C failure. */
    bool (*saveDab)(std::uint8_t antCap);
    /** Diagnostic-only, 2026-08-23: re-run Si4684Driver::boot() with new
     *  crystal parameters (no ESP32 restart, no persistence). Bridged here
     *  rather than through a new context field to avoid a second plumbing
     *  chain for what is a temporary calibration tool.
     *  @return false if the chip was never booted or the reboot failed. */
    bool (*recalibrateXtal)(std::uint8_t ibias, std::uint8_t ctun,
                            std::uint32_t xtalFreqHz);
};

/**
 * @brief    HttpRouteContext — dependencies injected into HTTP handlers.
 *
 * @dname    HttpRouteContext
 * @return   n/a (type)
 * @pubstate Borrows store and tuner for the lifetime of SetupWebServer.
 *           Passed as esp_http_server user_ctx (no file-scope globals).
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
struct HttpRouteContext {
    core::ISecureStore* store;        ///< Secure store for Wi-Fi provisioning.
    tuner::TunerService* tuner;       ///< Tuner service for tuner REST routes.
    audio::AudioService* audio;       ///< Audio service for ADAU1701 REST routes.
    bluetooth::BluetoothService* bluetooth; ///< Bluetooth pairing REST routes.
    station::StationService* stations; ///< Preset list REST routes.
    integration::IntegrationService* integration; ///< Preset recall orchestration.
    ota::OtaService* ota;                         ///< Firmware OTA streaming.
    webradio::WebRadioService* webRadio; ///< Streaming config REST routes.
    PhoneStreamSink* phoneStream; ///< PUT /api/stream/phone I2S write-through.
    AntennaCalibration* antennaCalibration; ///< POST /api/tuner/calibrate-antenna.
    core::CompanionChipStatus companionChips; ///< Boot flags for /api/health.
    core::DeviceIdentity deviceIdentity;       ///< EEPROM-derived unit identity.
};

/**
 * @brief    SetupWebServer — setup UI, health, and Wi-Fi provisioning API.
 *
 * @dname    SetupWebServer
 * @return   n/a (type)
 * @pubstate Owns server_ and borrows store_ while running. Routes delegate
 *           JSON work to the pure core.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class SetupWebServer {
public:
    /**
     * @brief    SetupWebServer — construct an unstarted server.
     *
     * @dname    SetupWebServer
     * @pubstate clears server_, store_, and netState_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer();

    /**
     * @brief    ~SetupWebServer — stop the HTTP server if running.
     *
     * @dname    ~SetupWebServer
     * @pubstate stops server_ when non-null.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    ~SetupWebServer();

    SetupWebServer(const SetupWebServer&) = delete;
    SetupWebServer& operator=(const SetupWebServer&) = delete;

    /**
     * @brief    SetupWebServer — move-construct, transferring the handle.
     *
     * @dname    SetupWebServer
     * @param    other  Source server; left stopped after the move.
     * @pubstate takes ownership of other.server_ and copies store pointer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer(SetupWebServer&& other) noexcept;

    /**
     * @brief    operator= — move-assign, transferring the handle.
     *
     * @dname    operator=
     * @param    other  Source server; left stopped after the move.
     * @return   Reference to this instance.
     * @pubstate takes ownership of other.server_ and copies store pointer.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    SetupWebServer& operator=(SetupWebServer&& other) noexcept;

    /**
     * @brief    start — register routes and listen on port 80.
     *
     * @dname    start
     * @param    store     Secure store for POST /api/wifi persistence.
     * @param    netState  Active network phase exposed to handlers.
     * @param    tuner     Tuner service for the tuner REST routes.
     * @param    audio     Audio service for the audio REST routes.
     * @param    bluetooth       Bluetooth service for pairing REST routes.
     * @param    stations        Station preset service for list REST routes.
     * @param    integration     Application orchestration for preset recall.
     * @param    ota             Firmware OTA service for POST /api/system/ota.
     * @param    webRadio        Streaming config for GET/POST /api/streaming.
     * @param    phoneStream     I2S write-through for PUT /api/stream/phone.
     * @param    antennaCalibration  EEPROM write-through for
     *                                POST /api/tuner/calibrate-antenna.
     * @param    companionChips  Boot flags for GET /api/health.
     * @param    deviceIdentity  Unit identity for /api/health serialNumber.
     * @return   Ok on success, or NetError::HttpServerStartFailed.
     * @pubstate writes server_, store_, netState_, and service pointers on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, NetError> start(
        core::ISecureStore& store, NetState netState,
        tuner::TunerService& tuner, audio::AudioService& audio,
        bluetooth::BluetoothService& bluetooth,
        station::StationService& stations,
        integration::IntegrationService& integration,
        ota::OtaService& ota,
        webradio::WebRadioService& webRadio,
        PhoneStreamSink& phoneStream,
        AntennaCalibration& antennaCalibration,
        core::CompanionChipStatus companionChips,
        const core::DeviceIdentity& deviceIdentity);

private:
    httpd_handle_t server_;
    core::ISecureStore* store_;
    NetState netState_;
    tuner::TunerService* tuner_;
    audio::AudioService* audio_;
    bluetooth::BluetoothService* bluetooth_;
    station::StationService* stations_;
    integration::IntegrationService* integration_;
};

} // namespace net
