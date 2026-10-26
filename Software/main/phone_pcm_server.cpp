/**
 * @file    phone_pcm_server.cpp
 * @brief   Raw PCM audio from the phone on a dedicated TCP port.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "phone_pcm_server.hpp"

#include "audio_routing.hpp"
#include "phone_stream.hpp"
#include "system_diag.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "mdns.h"

#include <array>
#include <cstring>

namespace phone_pcm_server {

namespace {

constexpr char kTag[] = "phone_pcm";
constexpr std::size_t kFrameBytes = 4U; // int16 left + int16 right
constexpr std::size_t kRecvBytes = 4096U;
/** A paused phone app stops sending: give up the stream after this. */
constexpr int kRecvTimeoutS = 5;
constexpr int kSinkWaitTries = 30; // x 100 ms
constexpr std::uint32_t kStackBytes = 6144U;

[[nodiscard]] int listenSocket()
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }
    const int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/** The web radio releases the sink within a second of being disabled,
 *  which the app does just before connecting. */
[[nodiscard]] bool acquireSink()
{
    const net::PhoneStreamSink& sink = phone_stream::sink();
    for (int i = 0; i < kSinkWaitTries; ++i) {
        if (sink.tryAcquire()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return false;
}

/** Receives PCM until the client closes or stalls.
 *  @return bytes played. */
std::size_t pump(int fd)
{
    static std::array<std::uint8_t, kRecvBytes> buf;
    static std::array<std::int16_t, kRecvBytes / 2U> frames;
    const net::PhoneStreamSink& sink = phone_stream::sink();
    std::size_t leftover = 0U;
    std::size_t total = 0U;
    while (true) {
        const int n = recv(fd, buf.data() + leftover, buf.size() - leftover, 0);
        if (n <= 0) {
            return total;
        }
        const std::size_t available = leftover + static_cast<std::size_t>(n);
        const std::size_t usable = available - (available % kFrameBytes);
        std::memcpy(frames.data(), buf.data(), usable);
        if (!sink.writePcm16Stereo(frames.data(), usable / kFrameBytes)) {
            ESP_LOGE(kTag, "I2S write failed");
            return total;
        }
        total += usable;
        leftover = available - usable;
        std::memmove(buf.data(), buf.data() + usable, leftover);
    }
}

void serve(int fd)
{
    const timeval timeout{.tv_sec = kRecvTimeoutS, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (!acquireSink()) {
        ESP_LOGW(kTag, "audio output busy (web radio still on?)");
        return;
    }
    if (!audio_routing::claimEsp32Input()) {
        ESP_LOGW(kTag, "ADAU source switch failed; stream may be inaudible");
    }
    ESP_LOGI(kTag, "phone connected");
    const std::size_t bytes = pump(fd);
    ESP_LOGI(kTag, "phone disconnected after %u s of audio",
             static_cast<unsigned>(bytes / (48000U * kFrameBytes)));
    audio_routing::restorePreviousSource();
    phone_stream::sink().release();
}

[[noreturn]] void serverTask(void* /*arg*/)
{
    int listener = -1;
    while (listener < 0) {
        listener = listenSocket();
        if (listener < 0) {
            ESP_LOGE(kTag, "cannot listen on port %u; retrying", kPort);
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
    ESP_LOGI(kTag, "listening on TCP port %u", kPort);
    while (true) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        serve(client);
        shutdown(client, SHUT_RDWR);
        close(client);
    }
}

} // namespace

bool start()
{
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(serverTask, "phone_pcm", kStackBytes, nullptr, 5, &handle)
        != pdPASS) {
        return false;
    }
    system_diag::watchTask("phone_pcm", handle);
    // Best effort: mDNS is only up in STA mode, and the app can always
    // fall back to the address it already uses for HTTP.
    if (mdns_service_add(nullptr, "_digiradio-pcm", "_tcp", kPort, nullptr, 0)
        != ESP_OK) {
        ESP_LOGW(kTag, "mDNS advertisement not added");
    }
    return true;
}

} // namespace phone_pcm_server
