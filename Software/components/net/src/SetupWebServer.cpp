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
#include "core/FirmwareVersion.hpp"
#include "core/HealthStatus.hpp"
#include "core/HealthStatusJson.hpp"
#include "core/ParseError.hpp"
#include "core/SeekDirection.hpp"
#include "core/TunerJson.hpp"
#include "core/WifiProvisionJson.hpp"
#include "tuner/TunerService.hpp"
#include "audio/AudioService.hpp"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <string>

namespace net {

namespace {
constexpr char kTag[] = "SetupWebServer";
constexpr char kFirmwareVersion[] = "0.5.0";
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

[[nodiscard]] bool readRequestBody(httpd_req_t* req, std::array<char, 512>& body)
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
    const core::HealthStatus status =
        core::HealthStatus::ok(core::FirmwareVersion(kFirmwareVersion));
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
 * @brief    indexGetHandler — serve gzipped setup page from flash.
 *
 * @dname    indexGetHandler
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

} // namespace

SetupWebServer::SetupWebServer()
    : server_(nullptr)
    , store_(nullptr)
    , netState_(NetState::Uninitialized)
    , tuner_(nullptr)
    , audio_(nullptr)
    , routeContext_{nullptr, nullptr, nullptr}
{
}

SetupWebServer::SetupWebServer(SetupWebServer&& other) noexcept
    : server_(other.server_)
    , store_(other.store_)
    , netState_(other.netState_)
    , tuner_(other.tuner_)
    , audio_(other.audio_)
    , routeContext_(other.routeContext_)
{
    other.server_ = nullptr;
    other.store_ = nullptr;
    other.netState_ = NetState::Uninitialized;
    other.tuner_ = nullptr;
    other.audio_ = nullptr;
    other.routeContext_ = {nullptr, nullptr, nullptr};
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
        routeContext_ = other.routeContext_;
        other.server_ = nullptr;
        other.store_ = nullptr;
        other.netState_ = NetState::Uninitialized;
        other.tuner_ = nullptr;
        other.audio_ = nullptr;
        other.routeContext_ = {nullptr, nullptr, nullptr};
    }
    return *this;
}

SetupWebServer::~SetupWebServer()
{
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
    routeContext_ = {nullptr, nullptr, nullptr};
}

std::expected<void, NetError> SetupWebServer::start(
    core::ISecureStore& store,
    NetState netState,
    tuner::TunerService& tuner,
    audio::AudioService& audio)
{
    if (server_ != nullptr) {
        return {};
    }

    store_ = &store;
    netState_ = netState;
    tuner_ = &tuner;
    audio_ = &audio;
    routeContext_.store = &store;
    routeContext_.tuner = &tuner;
    routeContext_.audio = &audio;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed");
        routeContext_ = {nullptr, nullptr, nullptr};
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

    ESP_LOGI(kTag, "HTTP server listening on port 80");
    return {};
}

} // namespace net
