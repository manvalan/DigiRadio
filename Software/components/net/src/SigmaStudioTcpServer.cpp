/**
 * @file    SigmaStudioTcpServer.cpp
 * @brief   SigmaStudioTcpServer implementation.
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
 * @date    2026-08-07
 */

#include "net/SigmaStudioTcpServer.hpp"

#include "adau1701/FlashDspProgramSource.hpp"

#include "core/DspProgram.hpp"
#include "core/DspProgramBlob.hpp"
#include "core/RegisterWrite.hpp"

#include "SigmaStudioFW.h"

#include "esp_log.h"

#include "lwip/sockets.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

namespace net {

namespace {

constexpr char kTag[] = "SigmaTcp";
constexpr std::uint16_t kPort = 8086U;
constexpr std::size_t kRecvBufSize = 16U * 1024U;
constexpr std::uint32_t kTaskStackBytes = 8192U;
constexpr UBaseType_t kTaskPriority = 5U;

constexpr std::uint8_t kCtrlWrite = 0x09U;
constexpr std::uint8_t kCtrlReadReq = 0x0AU;
constexpr std::uint8_t kCtrlReadResp = 0x0BU;
constexpr std::uint8_t kChipAddrDsp = 0x01U;
// board::pins::Adau1701Addr (main/board_pins.hpp), duplicated as a literal
// to avoid a net -> main include dependency; keep in sync if the board's
// I2C address ever changes. The reference project accepts both the IC
// index (0x01) and the raw address as chipAddr since it's unclear which
// convention real SigmaStudio uses for a single-IC project — mirrored here.
constexpr std::uint8_t kDspI2cAddr7 = 0x34U;

[[nodiscard]] bool isDspChipAddr(std::uint8_t chipAddr) noexcept
{
    return chipAddr == kChipAddrDsp || chipAddr == kDspI2cAddr7;
}

/**
 * @brief    activeListenFd — process-lifetime storage for the listen fd.
 *
 * @dname    activeListenFd
 * @return   Reference to the singleton listen-fd slot.
 * @pubstate Written by SigmaStudioTcpServer::start()/stop(); read by
 *           acceptLoopTask(). SigmaStudioTcpServer is constructed as a
 *           local and move-relocated into NetBootstrap (see
 *           NetBootstrap.cpp), so a `this` pointer captured at start()
 *           time would go stale once that local's stack frame returns —
 *           same problem SetupWebServer's routeContextStorage() solves.
 *
 * @author   Michele Bigi
 * @date     2026-08-07
 */
[[nodiscard]] std::atomic<int>& activeListenFd() noexcept
{
    static std::atomic<int> fd{-1};
    return fd;
}

constexpr std::uint16_t kProgRamStart = 0x0400U;
constexpr std::uint16_t kProgRamEnd = 0x07FFU;
constexpr std::uint16_t kCtrlRegStart = 0x0800U;
constexpr std::uint16_t kCoreControlReg = 0x081CU;
constexpr std::uint8_t kDspRunBit = 0x04U;
constexpr unsigned kWordBytesParam = 4U;
constexpr unsigned kWordsPerSafeload = 5U;

constexpr std::size_t kWriteHeaderSize = 10U;
constexpr std::size_t kReadReqHeaderSize = 8U;
constexpr std::size_t kReadRespHeaderSize = 9U;
constexpr std::uint16_t kMaxReadBytes = 256U;

constexpr std::size_t kMaxCaptureRegions = 32U; // core::DspProgramBlob kMaxWriteCount
constexpr std::size_t kMaxRegionPayload = 16U * 1024U; // kMaxWritePayload

[[nodiscard]] unsigned wordSizeForAddress(std::uint16_t address) noexcept
{
    if (address >= kProgRamStart && address <= kProgRamEnd) {
        return 5U;
    }
    if (address >= kCtrlRegStart) {
        return 2U;
    }
    return 4U;
}

[[nodiscard]] std::uint16_t readBe16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8)
                                      | p[1]);
}

void writeBe16(std::uint8_t* p, std::uint16_t value) noexcept
{
    p[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFU);
    p[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

/**
 * @brief    PendingRegion — one coalesced contiguous write during a Download.
 */
struct PendingRegion {
    std::uint16_t address;
    std::vector<std::uint8_t> data;
};

/**
 * @brief    DownloadCapture — coalesces a Link Compile Download for persistence.
 *
 * Merges contiguous direct (non-safeload) DSP writes into a handful of
 * regions (mirroring the ~5-block shape of EmbeddedDspProgramSource), then
 * on finish() serialises and stores them via FlashDspProgramSource so the
 * downloaded program becomes what DigiRadio boots with next time. Bails
 * out (does not persist) if the session doesn't fit the DRAD blob's own
 * caps — the DSP still runs fine from what was already written live.
 */
class DownloadCapture {
public:
    void addWrite(std::uint16_t address, std::span<const std::uint8_t> data)
    {
        if (overflowed_ || data.empty()) {
            return;
        }
        if (tryExtendLast(address, data)) {
            return;
        }
        if (regions_.size() >= kMaxCaptureRegions) {
            abandon();
            return;
        }
        regions_.push_back(PendingRegion{
            address, std::vector<std::uint8_t>(data.begin(), data.end())});
    }

    void finish()
    {
        if (!overflowed_ && !regions_.empty()) {
            persist();
        }
        regions_.clear();
        overflowed_ = false;
    }

private:
    [[nodiscard]] bool tryExtendLast(std::uint16_t address,
                                     std::span<const std::uint8_t> data)
    {
        if (regions_.empty()) {
            return false;
        }
        PendingRegion& last = regions_.back();
        const unsigned wordSize = wordSizeForAddress(last.address);
        const auto lastWords =
            static_cast<std::uint32_t>(last.data.size() / wordSize);
        const std::uint32_t lastEnd =
            static_cast<std::uint32_t>(last.address) + lastWords;
        if (lastEnd != address
            || last.data.size() + data.size() > kMaxRegionPayload) {
            return false;
        }
        last.data.insert(last.data.end(), data.begin(), data.end());
        return true;
    }

    void abandon()
    {
        overflowed_ = true;
        regions_.clear();
        ESP_LOGW(kTag,
                 "Download too fragmented to persist as boot program — "
                 "DSP still runs live, only reboot-persistence is skipped");
    }

    void persist()
    {
        std::vector<core::RegisterWrite> writes;
        writes.reserve(regions_.size());
        for (auto& region : regions_) {
            writes.emplace_back(region.address, std::move(region.data));
        }
        const core::DspProgram program(std::move(writes));
        const std::vector<std::uint8_t> blob =
            core::serializeDspProgramBlob(program);
        if (auto stored = adau1701::FlashDspProgramSource::storeBlob(blob);
            !stored) {
            ESP_LOGW(kTag, "SigmaStudio download not persisted (flash store failed)");
            return;
        }
        ESP_LOGI(kTag,
                 "SigmaStudio download persisted as boot program (%u bytes)",
                 static_cast<unsigned>(blob.size()));
    }

    std::vector<PendingRegion> regions_;
    bool overflowed_ = false;
};

/**
 * @brief    ConnectionState — per-TCP-connection Download tracking.
 */
struct ConnectionState {
    DownloadCapture capture;
    bool dspRunning = false;
};

void directWrite(std::uint16_t address, std::span<const std::uint8_t> data)
{
    if (data.empty()) {
        return;
    }
    sigma_studio_lock();
    SIGMA_WRITE_REGISTER_BLOCK(
        0U, address, static_cast<unsigned int>(data.size()),
        const_cast<ADI_REG_TYPE*>(
            reinterpret_cast<const ADI_REG_TYPE*>(data.data())));
    sigma_studio_unlock();
}

void safeloadWrite(std::uint16_t address, std::span<const std::uint8_t> data)
{
    const auto totalWords =
        static_cast<unsigned>(data.size() / kWordBytesParam);
    unsigned offset = 0U;
    sigma_studio_lock();
    while (offset < totalWords) {
        const unsigned words =
            std::min(totalWords - offset, kWordsPerSafeload);
        unsigned addrs[kWordsPerSafeload];
        for (unsigned i = 0U; i < words; ++i) {
            addrs[i] = static_cast<unsigned>(address) + offset + i;
        }
        sigma_safeload_raw_block(
            static_cast<unsigned char>(words), addrs,
            data.data() + static_cast<std::size_t>(offset) * kWordBytesParam);
        offset += words;
    }
    sigma_studio_unlock();
}

void trackDownloadCompletion(std::uint16_t address,
                             std::span<const std::uint8_t> payload,
                             ConnectionState& state)
{
    if (address != kCoreControlReg || payload.size() < 2U) {
        return;
    }
    const bool wasRunning = state.dspRunning;
    state.dspRunning = (payload.back() & kDspRunBit) != 0U;
    if (!wasRunning && state.dspRunning) {
        state.capture.finish();
    }
}

void dispatchWrite(std::uint16_t address, std::span<const std::uint8_t> payload,
                   std::uint8_t safeload, ConnectionState& state)
{
    if (safeload != 0U) {
        safeloadWrite(address, payload);
        return;
    }
    if (!state.dspRunning) {
        state.capture.addWrite(address, payload);
    }
    directWrite(address, payload);
    trackDownloadCompletion(address, payload, state);
}

[[nodiscard]] std::size_t tryConsumeWrite(const std::uint8_t* p,
                                          std::size_t avail,
                                          ConnectionState& state)
{
    if (avail < kWriteHeaderSize) {
        return 0U;
    }
    const std::uint16_t totalLen = readBe16(p + 3U);
    if (totalLen < kWriteHeaderSize) {
        return 1U; // malformed frame — resync by one byte
    }
    if (avail < totalLen) {
        return 0U;
    }

    const std::uint8_t safeload = p[1];
    const std::uint8_t chipAddr = p[5];
    const std::uint16_t dataLen = readBe16(p + 6U);
    const std::uint16_t address = readBe16(p + 8U);

    const std::uint16_t maxPayload =
        static_cast<std::uint16_t>(totalLen - kWriteHeaderSize);
    const std::uint16_t safeLen = std::min(dataLen, maxPayload);

    if (isDspChipAddr(chipAddr)) {
        const std::span<const std::uint8_t> payload(p + kWriteHeaderSize,
                                                     safeLen);
        dispatchWrite(address, payload, safeload, state);
    }
    return totalLen;
}

void sendReadResponse(int clientFd, std::uint8_t chipAddr,
                      std::uint16_t address, std::uint16_t requested)
{
    const std::uint16_t length = std::min(requested, kMaxReadBytes);
    std::vector<std::uint8_t> data(length);

    sigma_studio_lock();
    const int result =
        length == 0U ? 0 : sigma_i2c_read(address, data.data(), length);
    sigma_studio_unlock();
    if (result != 0) {
        ESP_LOGW(kTag,
                 "read 0x%04x len=%u chipAddr=0x%02x: sigma_i2c_read failed",
                 static_cast<unsigned>(address),
                 static_cast<unsigned>(length),
                 static_cast<unsigned>(chipAddr));
        return;
    }

    std::vector<std::uint8_t> resp(kReadRespHeaderSize + length);
    resp[0] = kCtrlReadResp;
    writeBe16(resp.data() + 1U,
             static_cast<std::uint16_t>(kReadRespHeaderSize + length));
    resp[3] = chipAddr;
    writeBe16(resp.data() + 4U, length);
    writeBe16(resp.data() + 6U, address);
    resp[8] = 0x01U;
    std::memcpy(resp.data() + kReadRespHeaderSize, data.data(), length);
    const ssize_t sent = send(clientFd, resp.data(), resp.size(), 0);
    ESP_LOGI(kTag,
             "read 0x%04x len=%u chipAddr=0x%02x: sent %d/%u resp bytes",
             static_cast<unsigned>(address), static_cast<unsigned>(length),
             static_cast<unsigned>(chipAddr), static_cast<int>(sent),
             static_cast<unsigned>(resp.size()));
}

[[nodiscard]] std::size_t tryConsumeRead(const std::uint8_t* p,
                                         std::size_t avail, int clientFd)
{
    if (avail < kReadReqHeaderSize) {
        return 0U;
    }
    const std::uint16_t totalLen = readBe16(p + 1U);
    if (totalLen < kReadReqHeaderSize) {
        return 1U; // malformed frame — resync by one byte
    }
    if (avail < totalLen) {
        return 0U;
    }

    const std::uint8_t chipAddr = p[3];
    const std::uint16_t dataLen = readBe16(p + 4U);
    const std::uint16_t address = readBe16(p + 6U);
    if (isDspChipAddr(chipAddr)) {
        sendReadResponse(clientFd, chipAddr, address, dataLen);
    } else {
        ESP_LOGW(kTag,
                 "read req dropped: chipAddr=0x%02x not DSP (want 0x%02x or "
                 "0x%02x), addr=0x%04x len=%u",
                 static_cast<unsigned>(chipAddr),
                 static_cast<unsigned>(kChipAddrDsp),
                 static_cast<unsigned>(kDspI2cAddr7),
                 static_cast<unsigned>(address),
                 static_cast<unsigned>(dataLen));
    }
    return totalLen;
}

[[nodiscard]] std::size_t processBuffer(std::uint8_t* buf, std::size_t len,
                                        int clientFd, ConnectionState& state)
{
    std::size_t pos = 0U;
    while (pos < len) {
        const std::uint8_t ctrl = buf[pos];
        std::size_t consumed = 0U;
        if (ctrl == kCtrlWrite) {
            consumed = tryConsumeWrite(buf + pos, len - pos, state);
        } else if (ctrl == kCtrlReadReq) {
            consumed = tryConsumeRead(buf + pos, len - pos, clientFd);
        } else {
            ESP_LOGW(kTag, "unrecognized ctrl byte 0x%02x — resyncing",
                     static_cast<unsigned>(ctrl));
            consumed = 1U;
        }
        if (consumed == 0U) {
            break;
        }
        pos += consumed;
    }
    return pos;
}

/** @brief Diagnostic: hex-dump up to the first 48 bytes of a receive. */
void logRxHexDump(const std::uint8_t* p, std::size_t len)
{
    constexpr std::size_t kMaxDump = 48U;
    char hex[3U * kMaxDump + 1U];
    const std::size_t n = std::min(len, kMaxDump);
    for (std::size_t i = 0U; i < n; ++i) {
        std::snprintf(hex + i * 3U, 4U, "%02x ", static_cast<unsigned>(p[i]));
    }
    ESP_LOGI(kTag, "rx %u bytes: %s%s", static_cast<unsigned>(len), hex,
             len > kMaxDump ? "..." : "");
}

void serveClient(int clientFd)
{
    ConnectionState state;
    std::vector<std::uint8_t> buf(kRecvBufSize);
    std::size_t len = 0U;

    while (true) {
        const ssize_t received =
            recv(clientFd, buf.data() + len, buf.size() - len, 0);
        if (received <= 0) {
            break;
        }
        logRxHexDump(buf.data() + len, static_cast<std::size_t>(received));
        len += static_cast<std::size_t>(received);

        const std::size_t consumed = processBuffer(buf.data(), len, clientFd, state);
        if (consumed > 0U && consumed < len) {
            std::memmove(buf.data(), buf.data() + consumed, len - consumed);
        }
        len = consumed <= len ? len - consumed : 0U;
    }
}

} // namespace

SigmaStudioTcpServer::SigmaStudioTcpServer()
    : listenFd_(-1)
    , task_(nullptr)
{
}

SigmaStudioTcpServer::~SigmaStudioTcpServer()
{
    stop();
}

SigmaStudioTcpServer::SigmaStudioTcpServer(SigmaStudioTcpServer&& other) noexcept
    : listenFd_(other.listenFd_)
    , task_(other.task_)
{
    other.listenFd_ = -1;
    other.task_ = nullptr;
}

SigmaStudioTcpServer&
SigmaStudioTcpServer::operator=(SigmaStudioTcpServer&& other) noexcept
{
    if (this != &other) {
        stop();
        listenFd_ = other.listenFd_;
        task_ = other.task_;
        other.listenFd_ = -1;
        other.task_ = nullptr;
    }
    return *this;
}

void SigmaStudioTcpServer::stop() noexcept
{
    if (task_ != nullptr) {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    if (listenFd_ >= 0) {
        // Only clear the singleton if it still points at *our* fd: every
        // boot path constructs this as a named local, start()s it, then
        // moves it into NetBootstrap, so the moved-from local's own
        // destructor runs stop() right after. An unconditional
        // activeListenFd().store(-1) here used to stomp the atomic the
        // moved-to (real, running) instance had just inherited, making
        // acceptLoopTask() spin on accept(-1, ...) == EBADF forever from
        // the very first boot -- root cause of the 2026-08-25 field
        // observation, not a Wi-Fi-layer event.
        int expected = listenFd_;
        activeListenFd().compare_exchange_strong(expected, -1,
                                                 std::memory_order_acq_rel);
        close(listenFd_);
        listenFd_ = -1;
    }
}

std::expected<void, NetError> SigmaStudioTcpServer::start()
{
    if (task_ != nullptr) {
        return {};
    }

    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(kTag, "socket() failed");
        return std::unexpected(NetError::TcpServerStartFailed);
    }

    const int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || listen(fd, 1) != 0) {
        ESP_LOGE(kTag, "bind/listen failed");
        close(fd);
        return std::unexpected(NetError::TcpServerStartFailed);
    }
    listenFd_ = fd;
    activeListenFd().store(fd, std::memory_order_release);

    const BaseType_t created =
        xTaskCreate(&SigmaStudioTcpServer::acceptLoopTask, "sigma_tcp",
                   kTaskStackBytes, nullptr, kTaskPriority, &task_);
    if (created != pdPASS) {
        ESP_LOGE(kTag, "xTaskCreate failed");
        activeListenFd().store(-1, std::memory_order_release);
        close(listenFd_);
        listenFd_ = -1;
        task_ = nullptr;
        return std::unexpected(NetError::TcpServerStartFailed);
    }

    ESP_LOGI(kTag, "SigmaStudio TCP bridge listening on port %u",
            static_cast<unsigned>(kPort));
    return {};
}

namespace {

/**
 * @brief    recreateListenSocket — rebind a fresh listening socket on kPort.
 *
 * @dname    recreateListenSocket
 * @return   The new fd on success (also stored in activeListenFd()), or -1.
 * @pubstate closes the previous fd read from activeListenFd() if any, then
 *          publishes the new one.
 *
 * Self-healing counterpart to SigmaStudioTcpServer::start()'s socket setup.
 * The 2026-08-25 field observation (accept() spinning on errno=EBADF
 * forever) turned out to be a stop() lifetime bug, now fixed there: this
 * function is kept as a safety net in case the singleton is ever cleared
 * from underneath a running accept task by some future code path, not
 * because it is expected to fire in normal operation.
 */
[[nodiscard]] int recreateListenSocket() noexcept
{
    const int oldFd = activeListenFd().exchange(-1, std::memory_order_acq_rel);
    if (oldFd >= 0) {
        close(oldFd);
    }

    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(kTag, "recreateListenSocket: socket() failed");
        return -1;
    }

    const int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(kPort);

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0
        || listen(fd, 1) != 0) {
        ESP_LOGE(kTag, "recreateListenSocket: bind/listen failed, errno=%d",
                errno);
        close(fd);
        return -1;
    }

    activeListenFd().store(fd, std::memory_order_release);
    ESP_LOGW(kTag, "SigmaStudio TCP listen socket recreated after failure");
    return fd;
}

} // namespace

void SigmaStudioTcpServer::acceptLoopTask(void* /*arg*/)
{
    while (true) {
        const int listenFd = activeListenFd().load(std::memory_order_acquire);
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = accept(
            listenFd, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            // EBADF means the listen socket itself is gone -- retrying
            // accept() on the same fd forever can never recover from this,
            // unlike a transient per-call error, so rebuild the socket
            // instead of just backing off and looping.
            if (errno == EBADF) {
                ESP_LOGE(kTag,
                        "accept() failed: listen socket invalid (errno=%d) "
                        "-- recreating",
                        errno);
                (void)recreateListenSocket();
                vTaskDelay(pdMS_TO_TICKS(500));
            } else {
                ESP_LOGW(kTag, "accept() failed: errno=%d", errno);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }
        ESP_LOGI(kTag, "SigmaStudio client connected");
        serveClient(clientFd);
        ESP_LOGI(kTag, "SigmaStudio client disconnected");
        close(clientFd);
    }
}

} // namespace net
