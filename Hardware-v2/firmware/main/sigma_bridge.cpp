/**
 * @file    sigma_bridge.cpp
 * @brief   SigmaStudio+ TCP/IP protocol over Adau1452Driver.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Packets per the Analog Devices wiki (SigmaStudio+, TCP/IP platform); byte
 * layout and big-endian order as in jusst-engineering/sigma_tcp, which drives
 * an ADAU1452. v1's bridge speaks the older 16-bit SigmaStudio headers.
 *
 * @author  Michele Bigi
 * @date    2026-11-01
 */

#include "sigma_bridge.hpp"

#include "system_diag.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sigma_bridge {

namespace {

constexpr char kTag[] = "sigma_bridge";
constexpr std::uint16_t kPort = 8086U;
constexpr std::uint8_t kCmdWrite = 0x09U;
constexpr std::uint8_t kCmdRead = 0x0AU;
constexpr std::uint8_t kCmdResponse = 0x0BU;
/** Control, safeload, channel, total[4], chip, length[4], address[2]. */
constexpr std::size_t kWriteHeader = 14U;
/** Control, total[4], chip, length[4], address[2], reserved[2]. */
constexpr std::size_t kReadHeader = 14U;
constexpr std::uint32_t kMaxData = 64U * 1024U;
/** Wait after these writes, as in sigma_tcp: soft reset, hibernate, core
 *  start, PLL enable. The export's delay steps do not reach the wire. */
constexpr std::array<std::uint16_t, 4> kSlowRegisters = {0xF890U, 0xF400U,
                                                         0xF402U, 0xF003U};
constexpr int kSlowRegisterMs = 100;

adau1452::Adau1452Driver* gDsp = nullptr;

[[nodiscard]] std::uint32_t be32(const std::uint8_t* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24)
           | (static_cast<std::uint32_t>(p[1]) << 16)
           | (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

[[nodiscard]] std::uint16_t be16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

void putBe32(std::uint8_t* p, std::uint32_t value) noexcept
{
    p[0] = static_cast<std::uint8_t>(value >> 24);
    p[1] = static_cast<std::uint8_t>(value >> 16);
    p[2] = static_cast<std::uint8_t>(value >> 8);
    p[3] = static_cast<std::uint8_t>(value);
}

[[nodiscard]] bool recvAll(int fd, std::uint8_t* data, std::size_t size)
{
    std::size_t got = 0U;
    while (got < size) {
        const ssize_t n = recv(fd, data + got, size - got, 0);
        if (n <= 0) {
            return false;
        }
        got += static_cast<std::size_t>(n);
    }
    return true;
}

[[nodiscard]] bool sendAll(int fd, const std::uint8_t* data, std::size_t size)
{
    std::size_t sent = 0U;
    while (sent < size) {
        const ssize_t n = send(fd, data + sent, size - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

void waitIfSlowRegister(std::uint16_t address)
{
    for (const std::uint16_t slow : kSlowRegisters) {
        if (address == slow) {
            vTaskDelay(pdMS_TO_TICKS(kSlowRegisterMs));
            return;
        }
    }
}

[[nodiscard]] bool serveWrite(int fd)
{
    std::array<std::uint8_t, kWriteHeader - 1U> h = {};
    if (!recvAll(fd, h.data(), h.size())) {
        return false;
    }
    const bool safeload = h[0] != 0U;
    const std::uint32_t length = be32(&h[7]);
    const std::uint16_t address = be16(&h[11]);
    if (length > kMaxData) {
        ESP_LOGE(kTag, "write of %lu bytes refused",
                 static_cast<unsigned long>(length));
        return false;
    }
    std::vector<std::uint8_t> data(length);
    if (!recvAll(fd, data.data(), data.size())) {
        return false;
    }
    const auto result = safeload ? gDsp->safeloadRaw(address, data)
                                 : gDsp->writeRaw(address, data);
    if (!result) {
        ESP_LOGW(kTag, "write to 0x%04X failed", address);
    }
    waitIfSlowRegister(address);
    return true;
}

[[nodiscard]] bool serveRead(int fd)
{
    std::array<std::uint8_t, kReadHeader - 1U> h = {};
    if (!recvAll(fd, h.data(), h.size())) {
        return false;
    }
    const std::uint8_t chip = h[4];
    const std::uint32_t length = be32(&h[5]);
    if (length > kMaxData) {
        ESP_LOGE(kTag, "read of %lu bytes refused",
                 static_cast<unsigned long>(length));
        return false;
    }
    // Same layout as the request, with success in place of reserved[0].
    std::vector<std::uint8_t> response(kReadHeader + length);
    response[0] = kCmdResponse;
    putBe32(&response[1], static_cast<std::uint32_t>(response.size()));
    response[5] = chip;
    putBe32(&response[6], length);
    response[10] = h[9];
    response[11] = h[10];
    const auto read = gDsp->readRaw(
        be16(&h[9]), std::span(response).subspan(kReadHeader));
    response[12] = read ? 0U : 1U; // wiki: 0 success, 1 failure
    return sendAll(fd, response.data(), response.size());
}

void serveClient(int fd)
{
    std::uint8_t command = 0U;
    while (recvAll(fd, &command, 1U)) {
        bool alive = false;
        if (command == kCmdWrite) {
            alive = serveWrite(fd);
        } else if (command == kCmdRead) {
            alive = serveRead(fd);
        } else {
            ESP_LOGW(kTag, "unknown command 0x%02X", command);
        }
        if (!alive) {
            return;
        }
    }
}

[[nodiscard]] int openListener()
{
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return -1;
    }
    const int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(kPort);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void bridgeTask(void* /*arg*/)
{
    const int listener = openListener();
    if (listener < 0) {
        ESP_LOGE(kTag, "port %u not open", kPort);
        vTaskDelete(nullptr);
        return;
    }
    while (true) {
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(kTag, "SigmaStudio+ connected");
        const int yes = 1;
        setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        serveClient(client);
        close(client);
        ESP_LOGI(kTag, "SigmaStudio+ disconnected");
    }
}

} // namespace

bool start(adau1452::Adau1452Driver& dsp)
{
    gDsp = &dsp;
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(bridgeTask, "sigma_bridge", 6144, nullptr, 3, &handle)
        != pdPASS) {
        return false;
    }
    system_diag::watchTask("sigma_bridge", handle);
    return true;
}

} // namespace sigma_bridge
