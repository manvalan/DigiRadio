/**
 * @file    web_radio_fetch.cpp
 * @brief   Web radio network task: HTTP(S) connect, playlist resolution,
 *          ICY metadata removal, background reconnect.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "web_radio_session.hpp"

#include "core/IcyStreamFilter.hpp"

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace web_radio_stream {

namespace {

constexpr char kTag[] = "web_radio_net";
constexpr int kHttpTimeoutMs = 8000;
constexpr int kMaxRedirects = 4;
constexpr int kMaxPlaylistHops = 3;
constexpr std::size_t kPlaylistMax = 4096U;
constexpr std::size_t kReadChunk = 2048U;
constexpr TickType_t kSendTimeout = pdMS_TO_TICKS(200);
constexpr std::uint32_t kBackoffMs[] = {1000U, 2000U, 5000U, 10000U};
/** A station that is not MP3 will not become MP3 in a second. */
constexpr std::uint32_t kPermanentRetryMs = 30000U;
constexpr std::uint32_t kStackBytes = 10240U;

enum class Outcome {
    Ended,     ///< Connection closed or stalled: reconnect with back-off.
    Failed,    ///< Connect/HTTP failure: reconnect with back-off.
    Permanent, ///< Wrong kind of URL: retry only after a long pause.
    Resolved,  ///< Playlist resolved to a new URL: connect again now.
};

enum class Kind { Mp3, Playlist, Unsupported, NotAudio };

struct ResponseHeaders {
    std::string contentType;
    std::size_t metaInterval{0U};
};

struct Classified {
    Kind kind;
    std::string_view codec;
};

[[nodiscard]] std::string lowercase(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

esp_err_t onHttpEvent(esp_http_client_event_t* event)
{
    if (event->event_id != HTTP_EVENT_ON_HEADER || event->user_data == nullptr) {
        return ESP_OK;
    }
    auto* headers = static_cast<ResponseHeaders*>(event->user_data);
    const std::string key = lowercase(event->header_key);
    if (key == "content-type") {
        headers->contentType = lowercase(event->header_value);
    } else if (key == "icy-metaint") {
        headers->metaInterval = std::strtoul(event->header_value, nullptr, 10);
    }
    return ESP_OK;
}

[[nodiscard]] bool endsWith(std::string_view text, std::string_view tail)
{
    return text.size() >= tail.size()
           && text.substr(text.size() - tail.size()) == tail;
}

[[nodiscard]] Classified classify(std::string_view type, std::string_view url)
{
    const std::string path = lowercase(url.substr(0U, url.find('?')));
    if (type.find("mpegurl") != std::string_view::npos
        || type.find("scpls") != std::string_view::npos
        || endsWith(path, ".m3u") || endsWith(path, ".m3u8")
        || endsWith(path, ".pls")) {
        return {Kind::Playlist, ""};
    }
    if (type.find("aac") != std::string_view::npos
        || type.find("mp4") != std::string_view::npos) {
        return {Kind::Unsupported, "aac"};
    }
    if (type.find("ogg") != std::string_view::npos
        || type.find("opus") != std::string_view::npos
        || type.find("flac") != std::string_view::npos) {
        return {Kind::Unsupported, "ogg/flac"};
    }
    if (type.starts_with("text/")) {
        return {Kind::NotAudio, ""};
    }
    return {Kind::Mp3, "mp3"}; // audio/mpeg, octet-stream or no header
}

void setNet(Session& s, core::WebRadioState state, core::WebRadioFault fault,
            int httpStatus = 0, std::string_view codec = {})
{
    xSemaphoreTake(s.lock, portMAX_DELAY);
    s.netState = state;
    s.fault = fault;
    if (httpStatus != 0 || fault == core::WebRadioFault::None) {
        s.httpStatus = httpStatus;
    }
    if (!codec.empty()) {
        s.codec = std::string(codec);
    }
    xSemaphoreGive(s.lock);
}

void setTitle(Session& s, const std::optional<std::string>& title)
{
    xSemaphoreTake(s.lock, portMAX_DELAY);
    s.title = title;
    xSemaphoreGive(s.lock);
}

[[nodiscard]] esp_http_client_handle_t createClient(const std::string& url,
                                                    ResponseHeaders& headers)
{
    esp_http_client_config_t cfg{};
    cfg.url = url.c_str();
    cfg.timeout_ms = kHttpTimeoutMs;
    cfg.event_handler = onHttpEvent;
    cfg.user_data = &headers;
    cfg.buffer_size = 2048;
    // HTTPS needs a trust anchor; the bundle is a no-op for http://.
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client != nullptr) {
        esp_http_client_set_header(client, "Icy-MetaData", "1");
        esp_http_client_set_header(client, "User-Agent", "DigiRadio/0.10");
    }
    return client;
}

[[nodiscard]] bool isRedirect(int status)
{
    return status == 301 || status == 302 || status == 303 || status == 307
           || status == 308;
}

/** Opens the connection and follows redirects; returns the final status
 *  or -1 when the connection itself failed. */
[[nodiscard]] int openFollowingRedirects(esp_http_client_handle_t client,
                                         ResponseHeaders& headers)
{
    for (int hop = 0; hop <= kMaxRedirects; ++hop) {
        headers = ResponseHeaders{};
        if (esp_http_client_open(client, 0) != ESP_OK) {
            return -1;
        }
        (void)esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (!isRedirect(status)) {
            return status;
        }
        esp_http_client_set_redirection(client);
        esp_http_client_close(client);
    }
    return -1;
}

[[nodiscard]] std::string readPlaylist(esp_http_client_handle_t client)
{
    std::string body(kPlaylistMax, '\0');
    std::size_t used = 0U;
    while (used < body.size()) {
        const int n = esp_http_client_read(client, body.data() + used,
                                           static_cast<int>(body.size() - used));
        if (n <= 0) {
            break;
        }
        used += static_cast<std::size_t>(n);
    }
    body.resize(used);
    return body;
}

[[nodiscard]] Outcome resolvePlaylist(Session& s,
                                      esp_http_client_handle_t client,
                                      std::string& url)
{
    const std::string body = readPlaylist(client);
    if (core::isHlsPlaylist(body)) {
        ESP_LOGW(kTag, "%s is an HLS playlist (not supported yet)", url.c_str());
        setNet(s, core::WebRadioState::Error,
               core::WebRadioFault::UnsupportedFormat, 0, "hls");
        return Outcome::Permanent;
    }
    auto next = core::extractPlaylistStreamUrl(body);
    if (!next) {
        setNet(s, core::WebRadioState::Error, core::WebRadioFault::NotAudio);
        return Outcome::Permanent;
    }
    ESP_LOGI(kTag, "playlist -> %s", next->c_str());
    url = std::move(*next);
    return Outcome::Resolved;
}

/** Pushes bytes into the ring, waiting for room; false if asked to stop. */
[[nodiscard]] bool pushAudio(Session& s, const std::uint8_t* data,
                             std::size_t length)
{
    while (length > 0U && !s.stop.load()) {
        const std::size_t sent =
            xStreamBufferSend(s.ring, data, length, kSendTimeout);
        data += sent;
        length -= sent;
    }
    return !s.stop.load();
}

[[nodiscard]] Outcome pumpAudio(Session& s, esp_http_client_handle_t client,
                                std::size_t metaInterval)
{
    static std::array<std::uint8_t, kReadChunk> chunk;
    core::IcyStreamFilter filter(metaInterval);
    setNet(s, core::WebRadioState::Playing, core::WebRadioFault::None, 200,
           "mp3");
    while (!s.stop.load()) {
        const int n = esp_http_client_read(
            client, reinterpret_cast<char*>(chunk.data()), chunk.size());
        if (n <= 0) {
            ESP_LOGW(kTag, "stream ended or stalled (%d)", n);
            return Outcome::Ended;
        }
        const std::size_t audio = filter.filter(
            std::span<std::uint8_t>(chunk.data(), static_cast<std::size_t>(n)));
        if (filter.title() != s.title) {
            setTitle(s, filter.title());
        }
        if (!pushAudio(s, chunk.data(), audio)) {
            break;
        }
    }
    return Outcome::Ended;
}

[[nodiscard]] Outcome handleResponse(Session& s,
                                     esp_http_client_handle_t client,
                                     const ResponseHeaders& headers,
                                     std::string& url)
{
    const Classified type = classify(headers.contentType, url);
    switch (type.kind) {
    case Kind::Playlist:
        return resolvePlaylist(s, client, url);
    case Kind::Unsupported:
        ESP_LOGW(kTag, "%s: unsupported format '%s'", url.c_str(),
                 headers.contentType.c_str());
        setNet(s, core::WebRadioState::Error,
               core::WebRadioFault::UnsupportedFormat, 0, type.codec);
        return Outcome::Permanent;
    case Kind::NotAudio:
        ESP_LOGW(kTag, "%s is a web page (%s), not a stream", url.c_str(),
                 headers.contentType.c_str());
        setNet(s, core::WebRadioState::Error, core::WebRadioFault::NotAudio);
        return Outcome::Permanent;
    case Kind::Mp3:
        break;
    }
    ESP_LOGI(kTag, "connected: %s (%s, icy-metaint %u)", url.c_str(),
             headers.contentType.c_str(),
             static_cast<unsigned>(headers.metaInterval));
    return pumpAudio(s, client, headers.metaInterval);
}

[[nodiscard]] Outcome streamOnce(Session& s, std::string& url)
{
    setNet(s, core::WebRadioState::Connecting, core::WebRadioFault::None);
    ResponseHeaders headers;
    esp_http_client_handle_t client = createClient(url, headers);
    if (client == nullptr) {
        setNet(s, core::WebRadioState::Error, core::WebRadioFault::OutOfMemory);
        return Outcome::Failed;
    }
    Outcome outcome = Outcome::Failed;
    const int status = openFollowingRedirects(client, headers);
    if (status < 0) {
        ESP_LOGW(kTag, "connect failed: %s", url.c_str());
        setNet(s, core::WebRadioState::Error,
               core::WebRadioFault::ConnectFailed);
    } else if (status < 200 || status >= 300) {
        ESP_LOGW(kTag, "%s answered HTTP %d", url.c_str(), status);
        setNet(s, core::WebRadioState::Error, core::WebRadioFault::HttpStatus,
               status);
    } else {
        outcome = handleResponse(s, client, headers, url);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return outcome;
}

/** Sleeps in small steps so a stop request is honoured quickly. */
void pause(const Session& s, std::uint32_t ms)
{
    for (std::uint32_t waited = 0U; waited < ms && !s.stop.load();
         waited += 200U) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void countReconnect(Session& s)
{
    xSemaphoreTake(s.lock, portMAX_DELAY);
    ++s.reconnects;
    xSemaphoreGive(s.lock);
}

void fetchTask(void* arg)
{
    auto& s = *static_cast<Session*>(arg);
    std::string url = s.url;
    int hops = 0;
    std::size_t backoff = 0U;
    while (!s.stop.load()) {
        const Outcome outcome = streamOnce(s, url);
        if (outcome == Outcome::Resolved && ++hops <= kMaxPlaylistHops) {
            continue;
        }
        url = s.url;
        hops = 0;
        if (outcome == Outcome::Permanent) {
            pause(s, kPermanentRetryMs);
            continue;
        }
        backoff = outcome == Outcome::Ended ? 0U
                                            : std::min(backoff + 1U, 3U);
        pause(s, kBackoffMs[backoff]);
        countReconnect(s);
    }
    xSemaphoreGive(s.done);
    vTaskDelete(nullptr);
}

} // namespace

bool startFetch(Session& session)
{
    return xTaskCreate(fetchTask, "web_radio_net", kStackBytes, &session, 5,
                       nullptr)
           == pdPASS;
}

} // namespace web_radio_stream
