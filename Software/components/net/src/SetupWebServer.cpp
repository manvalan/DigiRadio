/**
 * @file    SetupWebServer.cpp
 * @brief   SetupWebServer implementation.
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

#include "net/SetupWebServer.hpp"

#include "core/AudioProfile.hpp"
#include "core/AudioProfileJson.hpp"
#include "core/BluetoothJson.hpp"
#include "core/CompanionChipStatus.hpp"
#include "core/DspProgramBlob.hpp"
#include "core/FirmwareVersion.hpp"
#include "core/HealthStatus.hpp"
#include "core/HealthStatusJson.hpp"
#include "core/IntegrationError.hpp"
#include "core/ParseError.hpp"
#include "core/SeekDirection.hpp"
#include "core/StationListJson.hpp"
#include "core/StoreError.hpp"
#include "core/TunerJson.hpp"
#include "core/WifiProvisionJson.hpp"
#include "tuner/TunerService.hpp"
#include "audio/AudioService.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "station/StationService.hpp"
#include "integration/IntegrationService.hpp"
#include "adau1701/FlashDspProgramSource.hpp"
#include "ota/OtaService.hpp"
#include "ota/OtaError.hpp"
#include "core/OtaAppDescriptor.hpp"
#include "bt1035/Bt1035Error.hpp"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace net {

namespace {
constexpr char kTag[] = "SetupWebServer";
constexpr char kFirmwareVersion[] = "0.8.3";
constexpr unsigned kRebootDelaySec = 3;

extern const uint8_t www_index_html_gz_start[] asm(
    "_binary_www_index_html_gz_start");
extern const uint8_t www_index_html_gz_end[] asm(
    "_binary_www_index_html_gz_end");

/**
 * @brief    routeContextFrom — read handler dependencies from user_ctx.
 *
 * @dname    routeContextFrom
 * @param    req  HTTP request handle from esp_http_server.
 * @return   Route context pointer, or nullptr when unset.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] HttpRouteContext* routeContextFrom(httpd_req_t* req) noexcept
{
    return static_cast<HttpRouteContext*>(httpd_req_get_user_ctx(req));
}

/**
 * @brief    rebootTask — restart after provisioning so STA mode can run.
 *
 * @dname    rebootTask
 * @param    arg  Unused.
 * @pubstate calls esp_restart.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
void rebootTask(void* arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(kRebootDelaySec * 1000));
    esp_restart();
}

/**
 * @brief    parseErrorToken — map ParseError to a safe API reason string.
 *
 * @dname    parseErrorToken
 * @param    error  Parse failure from the pure core.
 * @return   Short reason token without secrets.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
[[nodiscard]] const char* parseErrorToken(core::ParseError error) noexcept
{
    switch (error) {
    case core::ParseError::InvalidJson:
        return "invalid_json";
    case core::ParseError::MissingField:
        return "missing_field";
    case core::ParseError::InvalidSsid:
        return "invalid_ssid";
    case core::ParseError::InvalidPassword:
        return "invalid_password";
    }
    return "parse_error";
}

[[nodiscard]] const char* tunerErrorToken(core::TunerError error) noexcept
{
    switch (error) {
    case core::TunerError::NotBooted:
        return "not_booted";
    case core::TunerError::WrongBand:
        return "wrong_band";
    case core::TunerError::TuneFailed:
        return "tune_failed";
    case core::TunerError::ServiceListEmpty:
        return "service_list_empty";
    case core::TunerError::InvalidInput:
        return "invalid_input";
    case core::TunerError::HardwareFailed:
        return "hardware_failed";
    }
    return "tuner_error";
}

[[nodiscard]] const char* integrationErrorToken(
    core::IntegrationError error) noexcept
{
    switch (error) {
    case core::IntegrationError::StoreFailed:
        return "store_failed";
    case core::IntegrationError::PresetNotFound:
        return "not_found";
    case core::IntegrationError::TuneFailed:
        return "tune_failed";
    case core::IntegrationError::AudioFailed:
        return "audio_failed";
    }
    return "integration_error";
}

[[nodiscard]] const char* bt1035ErrorToken(bt1035::Bt1035Error error) noexcept
{
    switch (error) {
    case bt1035::Bt1035Error::NotBooted:
        return "not_booted";
    case bt1035::Bt1035Error::AtTimeout:
        return "at_timeout";
    case bt1035::Bt1035Error::AtError:
        return "at_error";
    case bt1035::Bt1035Error::UnexpectedResponse:
        return "unexpected_response";
    case bt1035::Bt1035Error::ResetFailed:
    case bt1035::Bt1035Error::UartInitFailed:
        return "driver_failed";
    }
    return "bluetooth_error";
}

template <std::size_t N>
[[nodiscard]] bool readRequestBodyImpl(httpd_req_t* req, std::array<char, N>& body)
{
    int received = 0;
    while (received < static_cast<int>(body.size()) - 1) {
        const int chunk = httpd_req_recv(req, body.data() + received,
                                         body.size() - 1 - received);
        if (chunk <= 0) {
            break;
        }
        received += chunk;
    }
    body[static_cast<std::size_t>(received)] = '\0';
    return received > 0;
}

template <std::size_t N>
[[nodiscard]] bool readRequestBody(httpd_req_t* req, std::array<char, N>& body)
{
    return readRequestBodyImpl(req, body);
}

/**
 * @brief    healthGetHandler — serve GET /api/health as JSON.
 *
 * @dname    healthGetHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate none; serialises HealthStatus via the pure core.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t healthGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    const core::CompanionChipStatus chips =
        ctx != nullptr
            ? ctx->companionChips
            : core::CompanionChipStatus{
                  .si4684Ready = false,
                  .adau1701Ready = false,
                  .bt1035Ready = false,
              };
    const core::HealthStatus status = core::HealthStatus::ok(
        core::FirmwareVersion(kFirmwareVersion), chips,
        ctx != nullptr ? ctx->deviceIdentity.serialNumber()
                       : std::string_view("unknown"));
    const std::string json = core::serializeHealthStatusJson(status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    tunerStatusGetHandler — serve GET /api/tuner/status as JSON.
 *
 * @dname    tunerStatusGetHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context tuner service.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t tunerStatusGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->tuner == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto status = ctx->tuner->refreshStatus();
    if (!status) {
        const std::string json =
            core::serializeTunerErrorJson(tunerErrorToken(status.error()));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    const std::string json = core::serializeTunerStatusJson(*status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    tunerServicesGetHandler — serve GET /api/tuner/services as JSON.
 *
 * @dname    tunerServicesGetHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context tuner service.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t tunerServicesGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->tuner == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto services = ctx->tuner->listDabServices();
    if (!services) {
        const std::string json =
            core::serializeTunerErrorJson(tunerErrorToken(services.error()));
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    const std::string json = core::serializeTunerServicesJson(*services);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    tunerTunePostHandler — accept POST /api/tuner/tune JSON.
 *
 * @dname    tunerTunePostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context tuner service.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t tunerTunePostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->tuner == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::array<char, 512> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }

    const auto parsed =
        core::parseTunerTuneJson(std::string_view(body.data()));
    if (!parsed) {
        const std::string json = core::serializeTunerErrorJson("invalid_json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    std::expected<void, core::TunerError> result = std::unexpected(
        core::TunerError::InvalidInput);
    if (parsed->band == core::TunerBand::Dab) {
        result = ctx->tuner->tuneDab(parsed->dabFreqIndex);
    } else if (parsed->fmFrequency) {
        result = ctx->tuner->tuneFm(*parsed->fmFrequency);
    }

    if (!result) {
        const std::string json =
            core::serializeTunerErrorJson(tunerErrorToken(result.error()));
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    auto status = ctx->tuner->refreshStatus();
    const std::string json = status
        ? core::serializeTunerStatusJson(*status)
        : std::string("{\"status\":\"ok\"}");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    tunerPlayPostHandler — accept POST /api/tuner/play JSON.
 *
 * @dname    tunerPlayPostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context tuner service.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t tunerPlayPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->tuner == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::array<char, 512> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }

    const auto parsed =
        core::parseTunerPlayJson(std::string_view(body.data()));
    if (!parsed) {
        const std::string json = core::serializeTunerErrorJson("invalid_json");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    if (auto play = ctx->tuner->playDabService(parsed->serviceId,
                                               parsed->componentId);
        !play) {
        const std::string json =
            core::serializeTunerErrorJson(tunerErrorToken(play.error()));
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"playing\"}", 20);
}

/**
 * @brief    tunerSeekPostHandler — accept POST /api/tuner/seek (FM up).
 *
 * @dname    tunerSeekPostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context tuner service with SeekDirection::Up.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t tunerSeekPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->tuner == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    auto freq = ctx->tuner->seekFm(core::SeekDirection::Up);
    if (freq) {
        const std::string json = std::string("{\"frequency_khz\":")
            + std::to_string(freq->value()) + "}";
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json =
        core::serializeTunerErrorJson(tunerErrorToken(freq.error()));
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    audioProfileGetHandler — serve GET /api/audio/profile JSON.
 *
 * @dname    audioProfileGetHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate reads route context audio service snapshot.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t audioProfileGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->audio == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    const std::string json =
        core::serializeAudioProfileJson(ctx->audio->currentProfile());
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    audioProfilePutHandler — accept PUT /api/audio/profile JSON.
 *
 * @dname    audioProfilePutHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate parses profile, safeloads ADAU1701, persists to NVS.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t audioProfilePutHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->audio == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::array<char, 2048> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }

    const auto parsed =
        core::parseAudioProfileJson(std::string_view(body.data()));
    if (!parsed) {
        const std::string json =
            core::serializeAudioErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    if (auto applied = ctx->audio->applyProfile(*parsed, true); !applied) {
        const std::string json = core::serializeAudioErrorJson("store_failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json = core::serializeAudioSavedJson();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    audioResetPostHandler — restore factory-flat profile.
 *
 * @dname    audioResetPostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate applies AudioProfile::factoryDefault() and persists to NVS.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t audioResetPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->audio == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    const core::AudioProfile defaults = core::AudioProfile::factoryDefault();
    if (auto applied = ctx->audio->applyProfile(defaults, true); !applied) {
        const std::string json = core::serializeAudioErrorJson("store_failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json = core::serializeAudioSavedJson();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

/**
 * @brief    audioEnhancePostHandler — apply stereo or bass enhancement level.
 *
 * @dname    audioEnhancePostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate parses level, updates AudioService, persists to NVS.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t audioEnhancePostHandler(httpd_req_t* req, bool stereo)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->audio == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::array<char, 512> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }

    const auto parsed = core::parseEnhanceLevelJson(std::string_view(body.data()));
    if (!parsed) {
        const std::string json =
            core::serializeAudioErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::expected<void, core::StoreError> applied = stereo
        ? ctx->audio->setStereoEnhance(*parsed, true)
        : ctx->audio->setBassEnhance(*parsed, true);
    if (!applied) {
        const std::string json = core::serializeAudioErrorJson("store_failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json = core::serializeAudioSavedJson();
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t audioStereoEnhancePostHandler(httpd_req_t* req)
{
    return audioEnhancePostHandler(req, true);
}

esp_err_t audioBassEnhancePostHandler(httpd_req_t* req)
{
    return audioEnhancePostHandler(req, false);
}

/**
 * @brief    indexGetHandler — serve gzipped setup page from flash.
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate none; reads embedded www/index.html.gz blob.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t indexGetHandler(httpd_req_t* req)
{
    const size_t length =
        static_cast<size_t>(www_index_html_gz_end - www_index_html_gz_start);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req,
                           reinterpret_cast<const char*>(www_index_html_gz_start),
                           length);
}

/**
 * @brief    wifiPostHandler — accept POST /api/wifi provisioning JSON.
 *
 * @dname    wifiPostHandler
 * @param    req  HTTP request handle from esp_http_server.
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate uses route context secure store for persistence.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t wifiPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->store == nullptr) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::array<char, 512> body{};
    int received = 0;
    while (received < static_cast<int>(body.size()) - 1) {
        const int chunk = httpd_req_recv(req, body.data() + received,
                                         body.size() - 1 - received);
        if (chunk <= 0) {
            break;
        }
        received += chunk;
    }
    body[static_cast<std::size_t>(received)] = '\0';

    const auto parsed =
        core::parseWifiProvisionJson(std::string_view(body.data()));
    if (!parsed) {
        const std::string json = core::serializeWifiProvisionErrorJson(
            parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    if (!ctx->store->saveWifiCredentials(parsed.value())) {
        const std::string json =
            core::serializeWifiProvisionErrorJson("store_failed");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json =
        core::serializeWifiProvisionSavedJson(kRebootDelaySec);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());

    xTaskCreate(rebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

/**
 * @brief    dspProgramPostHandler — store validated ADAU program blob to flash.
 *
 * @dname    dspProgramPostHandler
 * @param    req  HTTP request handle (raw application/octet-stream body).
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate writes the dsp partition; schedules reboot to apply on next boot.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
esp_err_t dspProgramPostHandler(httpd_req_t* req)
{
    constexpr int kMaxBlobSize = 200 * 1024;
    const int contentLen = req->content_len;
    if (contentLen <= 0 || contentLen > kMaxBlobSize) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_send(req, nullptr, 0);
    }

    std::vector<std::uint8_t> body(static_cast<std::size_t>(contentLen));
    int received = 0;
    while (received < contentLen) {
        const int chunk = httpd_req_recv(req,
                                         reinterpret_cast<char*>(body.data())
                                             + received,
                                         contentLen - received);
        if (chunk <= 0) {
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, nullptr, 0);
        }
        received += chunk;
    }

    const auto parsed = core::parseDspProgramBlob(body);
    if (!parsed) {
        const std::string json = std::string(R"({"error":")")
                                 + core::dspProgramErrorToken(parsed.error())
                                 + R"("})";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    if (auto stored = adau1701::FlashDspProgramSource::storeBlob(body); !stored) {
        const std::string json = std::string(R"({"error":")")
                                 + core::dspProgramErrorToken(stored.error())
                                 + R"("})";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json =
        std::string(R"({"status":"stored","reboot_sec":)")
        + std::to_string(kRebootDelaySec) + "}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    xTaskCreate(rebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

/**
 * @brief    otaPostHandler — stream firmware image into inactive OTA slot.
 *
 * @dname    otaPostHandler
 * @param    req  HTTP request handle (raw application/octet-stream body).
 * @return   ESP_OK on success, or an esp_err_t error code.
 * @pubstate writes inactive OTA partition; schedules reboot on success.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
esp_err_t otaPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->ota == nullptr) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, nullptr, 0);
    }

    constexpr int kMaxOtaSize = 0x1B0000;
    const int contentLen = req->content_len;
    if (contentLen <= 0 || contentLen > kMaxOtaSize) {
        httpd_resp_set_status(req, "413 Payload Too Large");
        return httpd_resp_send(req, nullptr, 0);
    }

    ota::OtaService& otaService = *ctx->ota;
    if (auto begun = otaService.beginStream(contentLen); !begun) {
        const std::string json = std::string(R"({"error":")")
                                 + ota::otaErrorToken(begun.error())
                                 + R"("})";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    std::array<std::uint8_t, 4096> chunk{};
    int received = 0;
    while (received < contentLen) {
        const int toRead = std::min(static_cast<int>(chunk.size()),
                                    contentLen - received);
        const int n = httpd_req_recv(req,
                                     reinterpret_cast<char*>(chunk.data()),
                                     toRead);
        if (n <= 0) {
            otaService.abort();
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, nullptr, 0);
        }

        if (auto written = otaService.writeChunk(
                {chunk.data(), static_cast<std::size_t>(n)});
            !written) {
            const char* token = ota::otaErrorToken(written.error());
            if (written.error() == ota::OtaError::WriteFailed) {
                token = core::otaImageErrorToken(otaService.lastImageError());
            }
            const std::string json =
                std::string(R"({"error":")") + token + R"("})";
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, json.c_str(), json.size());
        }
        received += n;
    }

    if (auto finished = otaService.finishStream(); !finished) {
        const std::string json = std::string(R"({"error":")")
                                 + ota::otaErrorToken(finished.error())
                                 + R"("})";
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }

    const std::string json =
        std::string(R"({"status":"stored","reboot_sec":)")
        + std::to_string(kRebootDelaySec) + "}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.size());
    xTaskCreate(rebootTask, "reboot", 2048, nullptr, 5, nullptr);
    return ESP_OK;
}

esp_err_t bluetoothStatusGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->bluetooth == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto status = ctx->bluetooth->refreshStatus();
    if (!status) {
        const std::string json =
            core::serializeBluetoothErrorJson(bt1035ErrorToken(status.error()));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    const std::string json = core::serializeBluetoothStatusJson(*status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t bluetoothPairPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->bluetooth == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    if (auto result = ctx->bluetooth->startPairing(); !result) {
        const std::string json =
            core::serializeBluetoothErrorJson(bt1035ErrorToken(result.error()));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"pairing\"}", 20);
}

esp_err_t bluetoothPairStopPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->bluetooth == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    if (auto result = ctx->bluetooth->stopPairing(); !result) {
        const std::string json =
            core::serializeBluetoothErrorJson(bt1035ErrorToken(result.error()));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"idle\"}", 17);
}

esp_err_t bluetoothDisconnectPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->bluetooth == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    if (auto result = ctx->bluetooth->disconnect(); !result) {
        const std::string json =
            core::serializeBluetoothErrorJson(bt1035ErrorToken(result.error()));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"disconnected\"}", 24);
}

esp_err_t stationsGetHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->stations == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    const std::string json =
        core::serializeStationListJson(ctx->stations->list());
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json.c_str(), json.size());
}

esp_err_t stationsPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->stations == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    std::array<char, 512> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto parsed = core::parseStationJson(body.data());
    if (!parsed) {
        const std::string json =
            core::serializeStationListErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    if (auto added = ctx->stations->add(std::move(*parsed)); !added) {
        const std::string json = core::serializeStationListErrorJson(
            core::stationListErrorToken(added.error()));
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"saved\"}", 18);
}

esp_err_t stationsRemovePostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->stations == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    std::array<char, 128> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto parsed = core::parseStationRemoveJson(body.data());
    if (!parsed) {
        const std::string json =
            core::serializeStationListErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    if (auto removed = ctx->stations->removeAt(parsed->index); !removed) {
        const std::string json = core::serializeStationListErrorJson(
            core::stationListErrorToken(removed.error()));
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"removed\"}", 20);
}

esp_err_t stationsReorderPostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->stations == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    std::array<char, 128> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto parsed = core::parseStationReorderJson(body.data());
    if (!parsed) {
        const std::string json =
            core::serializeStationListErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    if (auto moved = ctx->stations->reorder(parsed->fromIndex, parsed->toIndex);
        !moved) {
        const std::string json = core::serializeStationListErrorJson(
            core::stationListErrorToken(moved.error()));
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"reordered\"}", 22);
}

esp_err_t stationsTunePostHandler(httpd_req_t* req)
{
    auto* ctx = routeContextFrom(req);
    if (ctx == nullptr || ctx->integration == nullptr) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, nullptr, 0);
    }
    std::array<char, 128> body{};
    if (!readRequestBody(req, body)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, nullptr, 0);
    }
    auto parsed = core::parseStationRemoveJson(body.data());
    if (!parsed) {
        const std::string json =
            core::serializeStationListErrorJson(parseErrorToken(parsed.error()));
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    if (auto tuned = ctx->integration->recallPreset(parsed->index); !tuned) {
        const std::string json = core::serializeTunerErrorJson(
            integrationErrorToken(tuned.error()));
        if (tuned.error() == core::IntegrationError::PresetNotFound) {
            httpd_resp_set_status(req, "404 Not Found");
        } else {
            httpd_resp_set_status(req, "500 Internal Server Error");
        }
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, json.c_str(), json.size());
    }
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"tuned\"}", 18);
}

} // namespace

SetupWebServer::SetupWebServer()
    : server_(nullptr)
    , store_(nullptr)
    , netState_(NetState::Uninitialized)
    , tuner_(nullptr)
    , audio_(nullptr)
    , bluetooth_(nullptr)
    , stations_(nullptr)
    , integration_(nullptr)
    , routeContext_{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                    nullptr, {}}
{
}

SetupWebServer::SetupWebServer(SetupWebServer&& other) noexcept
    : server_(other.server_)
    , store_(other.store_)
    , netState_(other.netState_)
    , tuner_(other.tuner_)
    , audio_(other.audio_)
    , bluetooth_(other.bluetooth_)
    , stations_(other.stations_)
    , integration_(other.integration_)
    , routeContext_(other.routeContext_)
{
    other.server_ = nullptr;
    other.store_ = nullptr;
    other.netState_ = NetState::Uninitialized;
    other.tuner_ = nullptr;
    other.audio_ = nullptr;
    other.bluetooth_ = nullptr;
    other.stations_ = nullptr;
    other.integration_ = nullptr;
    other.routeContext_ = {nullptr, nullptr, nullptr, nullptr, nullptr,
                           nullptr, nullptr, {}, core::DeviceIdentity::unknown()};
}

SetupWebServer& SetupWebServer::operator=(SetupWebServer&& other) noexcept
{
    if (this != &other) {
        if (server_ != nullptr) {
            httpd_stop(server_);
        }
        server_ = other.server_;
        store_ = other.store_;
        netState_ = other.netState_;
        tuner_ = other.tuner_;
        audio_ = other.audio_;
        bluetooth_ = other.bluetooth_;
        stations_ = other.stations_;
        integration_ = other.integration_;
        routeContext_ = other.routeContext_;
        other.server_ = nullptr;
        other.store_ = nullptr;
        other.netState_ = NetState::Uninitialized;
        other.tuner_ = nullptr;
        other.audio_ = nullptr;
        other.bluetooth_ = nullptr;
        other.stations_ = nullptr;
        other.integration_ = nullptr;
        other.routeContext_ = {nullptr, nullptr, nullptr, nullptr, nullptr,
                               nullptr, {}, core::DeviceIdentity::unknown()};
    }
    return *this;
}

SetupWebServer::~SetupWebServer()
{
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
    routeContext_ = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                     nullptr, {}, core::DeviceIdentity::unknown()};
}

std::expected<void, NetError> SetupWebServer::start(
    core::ISecureStore& store,
    NetState netState,
    tuner::TunerService& tuner,
    audio::AudioService& audio,
    bluetooth::BluetoothService& bluetooth,
    station::StationService& stations,
    integration::IntegrationService& integration,
    ota::OtaService& ota,
    core::CompanionChipStatus companionChips,
    const core::DeviceIdentity& deviceIdentity)
{
    if (server_ != nullptr) {
        return {};
    }

    store_ = &store;
    netState_ = netState;
    tuner_ = &tuner;
    audio_ = &audio;
    bluetooth_ = &bluetooth;
    stations_ = &stations;
    integration_ = &integration;
    routeContext_.store = &store;
    routeContext_.tuner = &tuner;
    routeContext_.audio = &audio;
    routeContext_.bluetooth = &bluetooth;
    routeContext_.stations = &stations;
    routeContext_.integration = &integration;
    routeContext_.ota = &ota;
    routeContext_.companionChips = companionChips;
    routeContext_.deviceIdentity = deviceIdentity;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed");
        routeContext_ = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         nullptr, {}, core::DeviceIdentity::unknown()};
        return std::unexpected(NetError::HttpServerStartFailed);
    }

    void* routeCtx = &routeContext_;

    const httpd_uri_t healthUri = {
        .uri = "/api/health",
        .method = HTTP_GET,
        .handler = healthGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &healthUri);

    const httpd_uri_t indexUri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = indexGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &indexUri);

    const httpd_uri_t wifiUri = {
        .uri = "/api/wifi",
        .method = HTTP_POST,
        .handler = wifiPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &wifiUri);

    const httpd_uri_t tunerStatusUri = {
        .uri = "/api/tuner/status",
        .method = HTTP_GET,
        .handler = tunerStatusGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &tunerStatusUri);

    const httpd_uri_t tunerServicesUri = {
        .uri = "/api/tuner/services",
        .method = HTTP_GET,
        .handler = tunerServicesGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &tunerServicesUri);

    const httpd_uri_t tunerTuneUri = {
        .uri = "/api/tuner/tune",
        .method = HTTP_POST,
        .handler = tunerTunePostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &tunerTuneUri);

    const httpd_uri_t tunerPlayUri = {
        .uri = "/api/tuner/play",
        .method = HTTP_POST,
        .handler = tunerPlayPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &tunerPlayUri);

    const httpd_uri_t tunerSeekUri = {
        .uri = "/api/tuner/seek",
        .method = HTTP_POST,
        .handler = tunerSeekPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &tunerSeekUri);

    const httpd_uri_t audioProfileGetUri = {
        .uri = "/api/audio/profile",
        .method = HTTP_GET,
        .handler = audioProfileGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &audioProfileGetUri);

    const httpd_uri_t audioProfilePutUri = {
        .uri = "/api/audio/profile",
        .method = HTTP_PUT,
        .handler = audioProfilePutHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &audioProfilePutUri);

    const httpd_uri_t audioResetUri = {
        .uri = "/api/audio/reset",
        .method = HTTP_POST,
        .handler = audioResetPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &audioResetUri);

    const httpd_uri_t audioStereoEnhanceUri = {
        .uri = "/api/audio/stereo-enhance",
        .method = HTTP_POST,
        .handler = audioStereoEnhancePostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &audioStereoEnhanceUri);

    const httpd_uri_t audioBassEnhanceUri = {
        .uri = "/api/audio/bass-enhance",
        .method = HTTP_POST,
        .handler = audioBassEnhancePostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &audioBassEnhanceUri);

    const httpd_uri_t dspProgramUri = {
        .uri = "/api/dsp/program",
        .method = HTTP_POST,
        .handler = dspProgramPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &dspProgramUri);

    const httpd_uri_t otaUri = {
        .uri = "/api/system/ota",
        .method = HTTP_POST,
        .handler = otaPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &otaUri);

    const httpd_uri_t bluetoothStatusUri = {
        .uri = "/api/bluetooth/status",
        .method = HTTP_GET,
        .handler = bluetoothStatusGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &bluetoothStatusUri);

    const httpd_uri_t bluetoothPairUri = {
        .uri = "/api/bluetooth/pair",
        .method = HTTP_POST,
        .handler = bluetoothPairPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &bluetoothPairUri);

    const httpd_uri_t bluetoothPairStopUri = {
        .uri = "/api/bluetooth/pair/stop",
        .method = HTTP_POST,
        .handler = bluetoothPairStopPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &bluetoothPairStopUri);

    const httpd_uri_t bluetoothDisconnectUri = {
        .uri = "/api/bluetooth/disconnect",
        .method = HTTP_POST,
        .handler = bluetoothDisconnectPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &bluetoothDisconnectUri);

    const httpd_uri_t stationsGetUri = {
        .uri = "/api/stations",
        .method = HTTP_GET,
        .handler = stationsGetHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &stationsGetUri);

    const httpd_uri_t stationsPostUri = {
        .uri = "/api/stations",
        .method = HTTP_POST,
        .handler = stationsPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &stationsPostUri);

    const httpd_uri_t stationsRemoveUri = {
        .uri = "/api/stations/remove",
        .method = HTTP_POST,
        .handler = stationsRemovePostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &stationsRemoveUri);

    const httpd_uri_t stationsReorderUri = {
        .uri = "/api/stations/reorder",
        .method = HTTP_POST,
        .handler = stationsReorderPostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &stationsReorderUri);

    const httpd_uri_t stationsTuneUri = {
        .uri = "/api/stations/tune",
        .method = HTTP_POST,
        .handler = stationsTunePostHandler,
        .user_ctx = routeCtx,
    };
    httpd_register_uri_handler(server_, &stationsTuneUri);

    ESP_LOGI(kTag, "HTTP server listening on port 80");
    return {};
}

} // namespace net
