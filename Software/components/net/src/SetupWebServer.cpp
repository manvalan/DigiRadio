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

#include "core/FirmwareVersion.hpp"
#include "core/HealthStatus.hpp"
#include "core/HealthStatusJson.hpp"
#include "core/ParseError.hpp"
#include "core/WifiProvisionJson.hpp"

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
constexpr char kFirmwareVersion[] = "0.2.0";
constexpr unsigned kRebootDelaySec = 3;

SetupWebServer* gActiveServer = nullptr;
core::ISecureStore* gStore = nullptr;

extern const uint8_t www_index_html_gz_start[] asm(
    "_binary_www_index_html_gz_start");
extern const uint8_t www_index_html_gz_end[] asm(
    "_binary_www_index_html_gz_end");

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
 * @pubstate uses gActiveServer->store_ for persistence.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
esp_err_t wifiPostHandler(httpd_req_t* req)
{
    if (gStore == nullptr) {
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

    if (!gStore->saveWifiCredentials(parsed.value())) {
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
{
}

SetupWebServer::SetupWebServer(SetupWebServer&& other) noexcept
    : server_(other.server_)
    , store_(other.store_)
    , netState_(other.netState_)
{
    other.server_ = nullptr;
    other.store_ = nullptr;
    other.netState_ = NetState::Uninitialized;
    gActiveServer = this;
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
        other.server_ = nullptr;
        other.store_ = nullptr;
        other.netState_ = NetState::Uninitialized;
        gActiveServer = this;
    }
    return *this;
}

SetupWebServer::~SetupWebServer()
{
    if (gActiveServer == this) {
        gActiveServer = nullptr;
    }
    if (gStore == store_) {
        gStore = nullptr;
    }
    if (server_ != nullptr) {
        httpd_stop(server_);
        server_ = nullptr;
    }
}

std::expected<void, NetError> SetupWebServer::start(core::ISecureStore& store,
                                                    NetState netState)
{
    if (server_ != nullptr) {
        return {};
    }

    store_ = &store;
    netState_ = netState;
    gActiveServer = this;
    gStore = &store;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;

    if (httpd_start(&server_, &config) != ESP_OK) {
        ESP_LOGE(kTag, "httpd_start failed");
        gActiveServer = nullptr;
        gStore = nullptr;
        return std::unexpected(NetError::HttpServerStartFailed);
    }

    const httpd_uri_t healthUri = {
        .uri = "/api/health",
        .method = HTTP_GET,
        .handler = healthGetHandler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server_, &healthUri);

    const httpd_uri_t indexUri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = indexGetHandler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server_, &indexUri);

    const httpd_uri_t wifiUri = {
        .uri = "/api/wifi",
        .method = HTTP_POST,
        .handler = wifiPostHandler,
        .user_ctx = nullptr,
    };
    httpd_register_uri_handler(server_, &wifiUri);

    ESP_LOGI(kTag, "HTTP server listening on port 80");
    return {};
}

} // namespace net
