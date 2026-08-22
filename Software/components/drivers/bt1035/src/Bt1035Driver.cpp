/**
 * @file    Bt1035Driver.cpp
 * @brief   Bt1035Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "bt1035/Bt1035Driver.hpp"

#include "core/Bt1035ScannedDevice.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace bt1035 {

namespace {
constexpr char kTag[] = "Bt1035";
constexpr int kUartPort = 2;
constexpr int kBaudRate = 115200;
constexpr int kUartRxBuffer = 4096;
constexpr int kUartTxBuffer = 256;
constexpr int kResponseTimeoutMs = 2000;
constexpr int kPostUartMs = 100;
/** Datasheet §4.7: "From the OFF state, SYS_CTRL must be asserted for
 *  >20 ms to start power up." */
constexpr int kSysCtlAssertMs = 20;
/** Datasheet §4.8: "Reset Protection timeout (typically greater than
 *  ~1.8 s) causes the device to power down if VCHG is not present and
 *  SYS_CTRL is low." Held comfortably longer than that before each
 *  power-up assert, to guarantee a genuine full power-down rather than a
 *  pulse too short for the module's own protection timer to act on — see
 *  resetAndInitOnce()'s comment for why this now runs on every attempt,
 *  not just the first. */
constexpr int kSysCtlDeassertMs = 2500;
/** Measured live (2026-08-20, power/wiring confirmed sound with a
 *  multimeter — VBAT_IN/SYS_CTRL/1.8V_OUT/VDD_IO all correct, TX/RX pins
 *  verified via continuity): the module's spontaneous boot banner
 *  (+VER=FSC-BT1035,..., +DEVSTAT=1) doesn't appear until ~18.5s after
 *  RESET# releases — full BT stack init, not just the internal regulator.
 *  The previous 3500ms wait here was never enough for the module to say
 *  anything, so every prior boot attempt cut power and restarted before
 *  the module could finish booting even once.
 *
 *  boot() makes exactly one resetAndInitOnce() attempt per call (matching
 *  fd9d4ae's validated 5/5-clean-boot design): the BT1035 datasheet's own
 *  "Reset Protection timeout (typically >1.8s)" means a second
 *  SYS_CTRL/RESET pulse fired shortly after a failed attempt would not
 *  reliably reach a clean power-off state before repowering — an internal
 *  retry loop here risks re-interrupting the module mid bring-up, the
 *  same class of bug fixed in fd9d4ae (redundant AT+RESET). Retries now
 *  live one layer up, in hardware::bt1035RetryTask
 *  (main/hardware_bootstrap.cpp), which only re-invokes a full, clean
 *  boot() call — never re-pulses the pins faster than a whole boot cycle
 *  apart. */
constexpr int kBootBannerWaitMs = 25000;

void flushUartRx(int uartPort) noexcept
{
    uart_flush_input(static_cast<uart_port_t>(uartPort));
    std::array<char, 256> discard{};
    while (uart_read_bytes(static_cast<uart_port_t>(uartPort), discard.data(),
                           discard.size(), 0)
           > 0) {
    }
}

// Diagnostic only, run if the single boot attempt fails at 115200 (the
// datasheet's own default). AT+BAUD persists across RESET#/SYS_CTRL power
// cycles (programming guide §5.1.3), so a stray manual AT+BAUD or
// AT+RESTORE sent during earlier interactive testing could have left the
// module listening at a different rate than our fixed assumption — this
// sweep tells us if that's what's happening instead of guessing.
constexpr std::array<int, 8> kBaudProbeCandidates = {
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

void probeBaudRates(int uartPort) noexcept
{
    ESP_LOGW(kTag, "115200 unresponsive — sweeping baud rates");
    for (const int baud : kBaudProbeCandidates) {
        if (uart_set_baudrate(static_cast<uart_port_t>(uartPort), baud)
            != ESP_OK) {
            continue;
        }
        flushUartRx(uartPort);
        uart_write_bytes(static_cast<uart_port_t>(uartPort), "AT\r\n", 4);

        std::array<char, 64> buf{};
        const int n = uart_read_bytes(static_cast<uart_port_t>(uartPort),
                                      buf.data(), buf.size(),
                                      pdMS_TO_TICKS(500));
        if (n > 0) {
            ESP_LOGW(kTag, "baud probe: module responded at %d baud (%d bytes)",
                     baud, n);
            ESP_LOG_BUFFER_HEX(kTag, buf.data(), static_cast<std::size_t>(n));
        } else {
            ESP_LOGI(kTag, "baud probe: silent at %d baud", baud);
        }
    }
    uart_set_baudrate(static_cast<uart_port_t>(uartPort), kBaudRate);
    flushUartRx(uartPort);
}

// The BT1035 prints an unsolicited boot banner (+VER=..., +DEVSTAT=1, ...)
// once its full Bluetooth stack finishes initialising — this blocks for up
// to kBootBannerWaitMs waiting for it, since that's the real boot-complete
// signal (the module is otherwise silent and won't answer AT commands
// until this appears).
void logRawUartBoot(int uartPort) noexcept
{
    std::array<std::uint8_t, 128> buf{};
    const int n = uart_read_bytes(static_cast<uart_port_t>(uartPort), buf.data(),
                                   buf.size(), pdMS_TO_TICKS(kBootBannerWaitMs));
    if (n <= 0) {
        ESP_LOGW("Bt1035", "no spontaneous UART bytes after hardware reset");
        return;
    }
    ESP_LOG_BUFFER_HEX("Bt1035", buf.data(), static_cast<std::size_t>(n));
}

constexpr int kBrEdrScanTimeoutMs = 90000;
constexpr int kScanProgressLogMs = 5000;
constexpr int kScanIdleCompleteMs = 4000;
constexpr int kDefaultScanListenSeconds = 25;
constexpr unsigned long kDevStatScanComplete = 1U;

void logAtLine(std::string_view label, std::string_view line) noexcept
{
    std::string sanitized;
    sanitized.reserve(line.size());
    for (const char ch : line) {
        if (ch == '\r') {
            sanitized.append("\\r");
        } else if (ch == '\n') {
            sanitized.append("\\n");
        } else if (ch >= 0x20 && ch < 0x7F) {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('.');
        }
    }
    ESP_LOGI(kTag, "%.*s: %s", static_cast<int>(label.size()), label.data(),
             sanitized.c_str());
}

[[nodiscard]] bool responseHasScanEnd(std::string_view response) noexcept
{
    return response.find("+SCAN=E") != std::string_view::npos
           || response.find("+SCAN= E") != std::string_view::npos
           || response.find("+SCAN=END") != std::string_view::npos
           || response.find("+SCAN= END") != std::string_view::npos;
}

[[nodiscard]] bool responseHasScanEntries(std::string_view response) noexcept
{
    return response.find("+SCAN=") != std::string_view::npos
           || response.find("+SCAN =") != std::string_view::npos;
}

[[nodiscard]] bool responseHasDevStatScanComplete(
    std::string_view response) noexcept
{
    if (!responseHasScanEntries(response)) {
        return false;
    }
    constexpr std::string_view kPrefix = "+DEVSTAT=";
    std::size_t pos = 0;
    unsigned long lastValue = 0U;
    bool found = false;
    while ((pos = response.find(kPrefix, pos)) != std::string_view::npos) {
        const std::size_t start = pos + kPrefix.size();
        char* end = nullptr;
        const unsigned long raw =
            std::strtoul(response.data() + start, &end, 10);
        if (end != response.data() + start) {
            lastValue = raw;
            found = true;
        }
        pos = start + 1U;
    }
    return found && lastValue == kDevStatScanComplete;
}

[[nodiscard]] std::size_t countScanEntries(std::string_view response) noexcept
{
    std::size_t count = 0U;
    std::size_t pos = 0;
    while ((pos = response.find("+SCAN=", pos)) != std::string_view::npos) {
        const std::size_t valueStart = pos + 6U;
        if (valueStart < response.size() && response[valueStart] != 'E') {
            ++count;
        }
        pos = valueStart + 1U;
    }
    return count;
}

enum class ScanCollectReason {
    EndMarker,
    DevStatComplete,
    IdleAfterScan,
    TimeoutPartial,
};

[[nodiscard]] bool scanCollectionShouldStop(std::string_view response,
                                            std::size_t scanEntryCount,
                                            TickType_t lastRxTick,
                                            TickType_t now,
                                            TickType_t started,
                                            std::uint8_t minScanSeconds,
                                            ScanCollectReason* reason) noexcept
{
    if (!response.empty() && responseHasScanEnd(response)) {
        if (reason != nullptr) {
            *reason = ScanCollectReason::EndMarker;
        }
        return true;
    }

    const int elapsedMs = static_cast<int>(pdTICKS_TO_MS(now - started));
    const int minListenMs =
        minScanSeconds > 0U
            ? static_cast<int>(minScanSeconds) * 1000
            : kDefaultScanListenSeconds * 1000;
    const bool minListenElapsed = elapsedMs >= minListenMs;

    if (minListenElapsed && responseHasDevStatScanComplete(response)) {
        if (reason != nullptr) {
            *reason = ScanCollectReason::DevStatComplete;
        }
        return true;
    }
    if (minListenElapsed && scanEntryCount > 0U && lastRxTick > 0U
        && (now - lastRxTick) >= pdMS_TO_TICKS(kScanIdleCompleteMs)) {
        if (reason != nullptr) {
            *reason = ScanCollectReason::IdleAfterScan;
        }
        return true;
    }
    return false;
}

void logScanRawPayload(std::string_view label, std::string_view response) noexcept
{
    if (response.empty()) {
        ESP_LOGI(kTag, "%.*s (0 bytes)", static_cast<int>(label.size()),
                 label.data());
        return;
    }
    std::string sanitized;
    sanitized.reserve(std::min(response.size(), std::size_t{512U}));
    for (const char ch : response) {
        if (ch == '\r') {
            sanitized.append("\\r");
        } else if (ch == '\n') {
            sanitized.append("\\n");
        } else if (ch >= 0x20 && ch < 0x7F) {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('.');
        }
    }
    constexpr std::size_t kChunk = 480U;
    if (sanitized.size() <= kChunk) {
        ESP_LOGI(kTag, "%.*s (%d bytes): %s",
                 static_cast<int>(label.size()), label.data(),
                 static_cast<int>(response.size()), sanitized.c_str());
        return;
    }
    ESP_LOGI(kTag, "%.*s (%d bytes, part 1/%u): %.*s",
             static_cast<int>(label.size()), label.data(),
             static_cast<int>(response.size()),
             static_cast<unsigned>(
                 (sanitized.size() + kChunk - 1U) / kChunk),
             static_cast<int>(kChunk), sanitized.c_str());
    for (std::size_t off = kChunk; off < sanitized.size(); off += kChunk) {
        const std::size_t len = std::min(kChunk, sanitized.size() - off);
        ESP_LOGI(kTag, "%.*s (cont %u): %.*s",
                 static_cast<int>(label.size()), label.data(),
                 static_cast<unsigned>(off / kChunk + 1U),
                 static_cast<int>(len), sanitized.c_str() + off);
    }
}

void logScanUartChunk(int received, std::string_view chunk) noexcept
{
    const std::string label = "scan UART RX +" + std::to_string(received);
    logScanRawPayload(label, chunk);
}

[[nodiscard]] const char* scanCollectReasonLabel(
    ScanCollectReason reason) noexcept
{
    switch (reason) {
    case ScanCollectReason::EndMarker:
        return "+SCAN=E/end marker";
    case ScanCollectReason::DevStatComplete:
        return "+DEVSTAT=1";
    case ScanCollectReason::IdleAfterScan:
        return "UART idle after +SCAN";
    case ScanCollectReason::TimeoutPartial:
        return "timeout";
    }
    return "unknown";
}

void logScanParsedDevice(const core::Bt1035ScannedDevice& device) noexcept
{
    ESP_LOGI(kTag,
             "scan device[%u]: mac=%s name=%s rssi=%d dBm class=%s addrType=%u",
             static_cast<unsigned>(device.index), device.mac.c_str(),
             device.name.empty() ? "(no name)" : device.name.c_str(),
             static_cast<int>(device.rssiDbm),
             device.deviceClass.empty() ? "-" : device.deviceClass.c_str(),
             static_cast<unsigned>(device.addressType));
}
} // namespace

Bt1035Driver::Bt1035Driver(Bt1035Pins pins)
    : pins_(pins)
    , booted_(false)
    , uartInstalled_(false)
    , uartPort_(kUartPort)
{
}

Bt1035Driver::~Bt1035Driver()
{
    if (uartInstalled_) {
        uart_driver_delete(static_cast<uart_port_t>(uartPort_));
        uartInstalled_ = false;
    }
}

bool Bt1035Driver::isBooted() const noexcept
{
    return booted_;
}

std::expected<void, Bt1035Error> Bt1035Driver::ensureBooted() const
{
    if (!booted_) {
        return std::unexpected(Bt1035Error::NotBooted);
    }
    return {};
}

std::expected<std::string, Bt1035Error> Bt1035Driver::transmitAndCollect(
    std::string_view commandLine, int timeoutMs)
{
    const int written = uart_write_bytes(static_cast<uart_port_t>(uartPort_),
                                         commandLine.data(),
                                         commandLine.size());
    if (written < 0
        || static_cast<std::size_t>(written) != commandLine.size()) {
        return std::unexpected(Bt1035Error::UartInitFailed);
    }

    std::array<char, 128> buffer{};
    std::string accumulated;
    const TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);

    while (xTaskGetTickCount() < deadline) {
        const int received = uart_read_bytes(static_cast<uart_port_t>(uartPort_),
                                             buffer.data(), buffer.size(),
                                             pdMS_TO_TICKS(50));
        if (received > 0) {
            accumulated.append(buffer.data(), static_cast<std::size_t>(received));
            const core::Bt1035AtResponseKind kind =
                core::parseBt1035AtResponse(accumulated);
            if (kind == core::Bt1035AtResponseKind::Ok) {
                return accumulated;
            }
            if (kind == core::Bt1035AtResponseKind::Error) {
                return std::unexpected(Bt1035Error::AtError);
            }
        }
    }

    return std::unexpected(Bt1035Error::AtTimeout);
}

std::expected<std::string, Bt1035Error> Bt1035Driver::transmitAndCollectUntil(
    std::string_view commandLine, std::string_view endMarker, int timeoutMs,
    std::uint8_t minScanSeconds)
{
    logAtLine("scan UART TX", commandLine);

    const int written = uart_write_bytes(static_cast<uart_port_t>(uartPort_),
                                         commandLine.data(),
                                         commandLine.size());
    if (written < 0
        || static_cast<std::size_t>(written) != commandLine.size()) {
        ESP_LOGE(kTag, "scan UART TX failed (%d)", written);
        return std::unexpected(Bt1035Error::UartInitFailed);
    }

    std::array<char, 512> buffer{};
    std::string accumulated;
    accumulated.reserve(8192U);
    const TickType_t started = xTaskGetTickCount();
    const TickType_t deadline = started + pdMS_TO_TICKS(timeoutMs);
    TickType_t lastProgressLog = started;
    TickType_t lastRxTick = 0;
    std::size_t scanEntryCount = 0U;
    ScanCollectReason stopReason = ScanCollectReason::TimeoutPartial;
    bool stoppedEarly = false;

    ESP_LOGI(kTag, "======== BT scan inquiry begin (min %u s, timeout %d ms) ========",
             static_cast<unsigned>(minScanSeconds), timeoutMs);

    while (xTaskGetTickCount() < deadline) {
        const int received = uart_read_bytes(static_cast<uart_port_t>(uartPort_),
                                             buffer.data(), buffer.size(),
                                             pdMS_TO_TICKS(200));
        const TickType_t now = xTaskGetTickCount();
        if (received > 0) {
            const std::string_view chunk(buffer.data(),
                                         static_cast<std::size_t>(received));
            logScanUartChunk(received, chunk);
            accumulated.append(buffer.data(), static_cast<std::size_t>(received));
            lastRxTick = now;

            const std::size_t updatedScanCount =
                countScanEntries(accumulated);
            if (updatedScanCount > scanEntryCount) {
                ESP_LOGI(kTag, "scan +SCAN entry #%u (total %u)",
                         static_cast<unsigned>(updatedScanCount),
                         static_cast<unsigned>(updatedScanCount));
                scanEntryCount = updatedScanCount;
            }

            if (!endMarker.empty()
                && accumulated.find(endMarker) != std::string::npos) {
                stopReason = ScanCollectReason::EndMarker;
                stoppedEarly = true;
                ESP_LOGI(kTag, "scan end marker %.*s seen",
                         static_cast<int>(endMarker.size()), endMarker.data());
                break;
            }
            if (scanCollectionShouldStop(accumulated, scanEntryCount, lastRxTick,
                                         now, started, minScanSeconds,
                                         &stopReason)) {
                stoppedEarly = true;
                if (stopReason == ScanCollectReason::EndMarker) {
                    ESP_LOGI(kTag, "scan end marker +SCAN=E seen");
                } else if (stopReason == ScanCollectReason::DevStatComplete) {
                    ESP_LOGI(kTag,
                             "scan complete: +DEVSTAT=1 after %u +SCAN entr(ies)",
                             static_cast<unsigned>(scanEntryCount));
                }
                break;
            }
            if (core::parseBt1035AtResponse(accumulated)
                == core::Bt1035AtResponseKind::Error) {
                logScanRawPayload("scan ERROR response", accumulated);
                return std::unexpected(Bt1035Error::AtError);
            }
        } else if (scanEntryCount > 0U
                   && scanCollectionShouldStop(accumulated, scanEntryCount,
                                               lastRxTick, now, started,
                                               minScanSeconds, &stopReason)) {
            stoppedEarly = true;
            if (stopReason == ScanCollectReason::IdleAfterScan) {
                ESP_LOGI(kTag,
                         "scan complete: idle %d ms after last +SCAN (%u entr(ies))",
                         kScanIdleCompleteMs,
                         static_cast<unsigned>(scanEntryCount));
            }
            break;
        }

        if ((now - lastProgressLog) >= pdMS_TO_TICKS(kScanProgressLogMs)) {
            lastProgressLog = now;
            const int elapsedMs =
                static_cast<int>(pdTICKS_TO_MS(now - started));
            const int minListenMs =
                minScanSeconds > 0U
                    ? static_cast<int>(minScanSeconds) * 1000
                    : kDefaultScanListenSeconds * 1000;
            ESP_LOGI(kTag,
                     "scan progress: %d ms, %d bytes rx, %u +SCAN, min listen %d ms",
                     elapsedMs, static_cast<int>(accumulated.size()),
                     static_cast<unsigned>(scanEntryCount), minListenMs);
        }
    }

    const int elapsedMs =
        static_cast<int>(pdTICKS_TO_MS(xTaskGetTickCount() - started));
    if (stoppedEarly) {
        ESP_LOGI(kTag, "scan stopped: reason=%s, elapsed=%d ms, %u +SCAN entr(ies)",
                 scanCollectReasonLabel(stopReason), elapsedMs,
                 static_cast<unsigned>(scanEntryCount));
    } else {
        stopReason = ScanCollectReason::TimeoutPartial;
        ESP_LOGW(kTag, "scan stopped: reason=%s, elapsed=%d ms, %u +SCAN entr(ies)",
                 scanCollectReasonLabel(stopReason), elapsedMs,
                 static_cast<unsigned>(scanEntryCount));
    }
    logScanRawPayload("scan UART full RX", accumulated);

    const bool hasEnd = stoppedEarly
                            || (!endMarker.empty()
                                    ? accumulated.find(endMarker)
                                          != std::string::npos
                                    : responseHasScanEnd(accumulated));
    if (!hasEnd) {
        if (!responseHasScanEntries(accumulated)) {
            if (accumulated.find("+A2DPSTAT=2") != std::string_view::npos) {
                ESP_LOGW(kTag,
                         "scan blocked: module auto-connecting A2DP (disable LINKCFG)");
            }
            if (accumulated.find("+DEVSTAT=") != std::string_view::npos) {
                ESP_LOGW(kTag, "scan saw DEVSTAT events but no +SCAN= lines");
            }
            ESP_LOGW(kTag, "scan timeout (%d bytes rx, no +SCAN entries)",
                     static_cast<int>(accumulated.size()));
            (void)transmitAndCollect(core::buildBt1035StopScanLine(), 1000);
            return std::unexpected(Bt1035Error::AtTimeout);
        }
        ESP_LOGW(kTag, "scan timeout but %d bytes contain +SCAN entries — parsing partial",
                 static_cast<int>(accumulated.size()));
    }

    ESP_LOGI(kTag, "======== BT scan inquiry end ========");
    return accumulated;
}

std::expected<void, Bt1035Error> Bt1035Driver::transmitAndExpectOkLogged(
    std::string_view label, std::string_view commandLine)
{
    logAtLine(label, commandLine);
    auto collected = transmitAndCollect(commandLine);
    if (collected) {
        if (!collected->empty()) {
            const std::string rxLabel = std::string(label) + " RX";
            logScanRawPayload(rxLabel, *collected);
        }
        ESP_LOGI(kTag, "%.*s: OK", static_cast<int>(label.size()), label.data());
        return {};
    }
    if (collected.error() == Bt1035Error::AtTimeout) {
        ESP_LOGW(kTag, "%.*s: timeout (no response)", static_cast<int>(label.size()),
                 label.data());
    } else {
        ESP_LOGW(kTag, "%.*s: failed (%d)", static_cast<int>(label.size()),
                 label.data(), static_cast<int>(collected.error()));
    }
    return std::unexpected(collected.error());
}

void Bt1035Driver::prepareForInquiryScan()
{
    ESP_LOGI(kTag, "======== BT scan prep begin ========");
    flushUartRx(uartPort_);
    (void)transmitAndExpectOkLogged("scan prep PRINT",
                                    core::buildBt1035EnablePrintLine());
    (void)transmitAndExpectOkLogged("scan prep LINKCFG off",
                                    core::buildBt1035DisableAutoLinkLine());
    (void)transmitAndExpectOkLogged("scan prep AUTOCONN off",
                                    core::buildBt1035SetAutoConnLine(0U));
    (void)transmitAndExpectOkLogged("scan prep PLIST clear",
                                    core::buildBt1035ClearPairedListLine());
    (void)transmitAndExpectOkLogged(
        "scan prep A2DPDISC",
        core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpDisconnect));
    (void)transmitAndExpectOkLogged("scan prep DSCA",
                                  core::buildBt1035DisconnectAllLine());
    (void)transmitAndExpectOkLogged(
        "scan prep PAIR=0",
        core::buildBt1035AtLine(core::Bt1035AtCommand::PairHidden));
    auto stopScan = transmitAndCollect(core::buildBt1035StopScanLine(), 1000);
    if (stopScan) {
        logScanRawPayload("scan prep SCAN=0 RX", *stopScan);
        ESP_LOGI(kTag, "scan prep SCAN=0: OK");
    } else {
        ESP_LOGI(kTag, "scan prep SCAN=0: skipped (no active scan)");
    }
    flushUartRx(uartPort_);
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!waitForA2dpIdle(10000)) {
        ESP_LOGW(kTag, "scan prep: A2DP still busy — continuing anyway");
    }
    if (auto paired = queryPairedList(); paired && !paired->empty()) {
        ESP_LOGI(kTag, "scan prep: %u paired device(s) on module",
                 static_cast<unsigned>(paired->size()));
        for (const core::Bt1035PairedDevice& entry : *paired) {
            ESP_LOGI(kTag, "scan prep paired: %s (%s)",
                     entry.mac.c_str(),
                     entry.name.empty() ? "(no name)" : entry.name.c_str());
        }
    }
    flushUartRx(uartPort_);
    ESP_LOGI(kTag, "======== BT scan prep end ========");
}

bool Bt1035Driver::waitForA2dpIdle(int timeoutMs)
{
    const TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
    TickType_t lastDisconnectAttempt = 0;
    while (xTaskGetTickCount() < deadline) {
        auto state = queryA2dpState();
        if (!state) {
            ESP_LOGW(kTag, "scan prep: A2DPSTAT query failed");
            return false;
        }
        ESP_LOGI(kTag, "scan prep: A2DPSTAT=%u",
                 static_cast<unsigned>(static_cast<std::uint8_t>(*state)));
        if (*state == core::Bt1035A2dpState::Standby
            || *state == core::Bt1035A2dpState::Unsupported) {
            return true;
        }
        const TickType_t now = xTaskGetTickCount();
        if ((now - lastDisconnectAttempt) >= pdMS_TO_TICKS(1500)) {
            lastDisconnectAttempt = now;
            ESP_LOGW(kTag, "scan prep: A2DP busy — sending A2DPDISC+DSCA");
            (void)transmitAndExpectOk(
                core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpDisconnect));
            (void)transmitAndExpectOk(core::buildBt1035DisconnectAllLine());
            flushUartRx(uartPort_);
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    return false;
}

std::expected<std::vector<core::Bt1035ScannedDevice>, Bt1035Error>
Bt1035Driver::runInquiryScan(std::uint8_t scanType, std::uint8_t scanSeconds)
{
    const std::string startLine =
        core::buildBt1035StartScanLine(scanType, scanSeconds);
    const int timeoutMs = scanSeconds == 0U
                              ? kBrEdrScanTimeoutMs
                              : static_cast<int>(scanSeconds) * 1000 + 30000;
    flushUartRx(uartPort_);
    ESP_LOGI(kTag, "scan inquiry: type=%u seconds=%u timeout=%d ms",
             static_cast<unsigned>(scanType),
             static_cast<unsigned>(scanSeconds), timeoutMs);
    auto response = transmitAndCollectUntil(startLine, {}, timeoutMs, scanSeconds);
    if (!response) {
        ESP_LOGW(kTag, "scan inquiry failed (type %u)",
                 static_cast<unsigned>(scanType));
        return std::unexpected(response.error());
    }

    auto parsed = core::parseBt1035ScanResponse(*response);
    if (!parsed) {
        ESP_LOGW(kTag, "scan parse failed (type %u)",
                 static_cast<unsigned>(scanType));
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    ESP_LOGI(kTag, "scan parsed %u device(s)",
             static_cast<unsigned>(parsed->size()));
    for (const core::Bt1035ScannedDevice& device : *parsed) {
        logScanParsedDevice(device);
    }
    return *parsed;
}

std::expected<void, Bt1035Error> Bt1035Driver::transmitAndExpectOk(
    std::string_view commandLine)
{
    auto collected = transmitAndCollect(commandLine);
    if (collected) {
        return {};
    }
    return std::unexpected(collected.error());
}

std::expected<void, Bt1035Error> Bt1035Driver::sendCommand(
    core::Bt1035AtCommand command)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return transmitAndExpectOk(core::buildBt1035AtLine(command));
}

std::expected<void, Bt1035Error> Bt1035Driver::enterPairingMode()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::PairDiscoverable);
}

std::expected<void, Bt1035Error> Bt1035Driver::leavePairingMode()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::PairHidden);
}

std::expected<core::Bt1035A2dpState, Bt1035Error> Bt1035Driver::queryA2dpState()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }

    auto response =
        transmitAndCollect(core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpStat));
    if (!response) {
        return std::unexpected(response.error());
    }

    auto parsed = core::parseBt1035A2dpStatResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<core::Bt1035A2dpCodec, Bt1035Error> Bt1035Driver::queryA2dpEncoder()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }

    auto response = transmitAndCollect(
        core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpEncoder));
    if (!response) {
        return std::unexpected(response.error());
    }

    auto parsed = core::parseBt1035A2dpEncoderResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<void, Bt1035Error> Bt1035Driver::disconnectA2dp()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::A2dpDisconnect);
}

std::expected<void, Bt1035Error> Bt1035Driver::setDeviceName(
    std::string_view name)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (name.empty() || name.size() > 31U) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    const std::string line =
        core::buildBt1035SetNameLine(name, false);
    if (line.empty()) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return transmitAndExpectOk(line);
}

std::expected<std::string, Bt1035Error> Bt1035Driver::queryDeviceName()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }
    auto response =
        transmitAndCollect(core::buildBt1035AtLine(core::Bt1035AtCommand::QueryName));
    if (!response) {
        return std::unexpected(response.error());
    }
    auto parsed = core::parseBt1035NameResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<void, Bt1035Error> Bt1035Driver::setAutoReconnect(
    std::uint8_t times)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return transmitAndExpectOk(core::buildBt1035SetAutoConnLine(times));
}

std::expected<std::uint8_t, Bt1035Error> Bt1035Driver::queryAutoReconnect()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }
    auto response = transmitAndCollect(
        core::buildBt1035AtLine(core::Bt1035AtCommand::QueryAutoConn));
    if (!response) {
        return std::unexpected(response.error());
    }
    auto parsed = core::parseBt1035AutoConnResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<std::vector<core::Bt1035PairedDevice>, Bt1035Error>
Bt1035Driver::queryPairedList()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }
    auto response = transmitAndCollect(
        core::buildBt1035AtLine(core::Bt1035AtCommand::QueryPairedList),
        4000);
    if (!response) {
        return std::unexpected(response.error());
    }
    auto parsed = core::parseBt1035PairedListResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<std::vector<core::Bt1035ScannedDevice>, Bt1035Error>
Bt1035Driver::scanNearbyBrEdr(std::uint8_t scanSeconds)
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }

    ESP_LOGI(kTag, "======== BT scan session begin (%u s) ========",
             static_cast<unsigned>(scanSeconds));

    prepareForInquiryScan();

    auto result = runInquiryScan(1U, scanSeconds);

    ESP_LOGI(kTag, "scan post-cleanup: A2DPDISC+DSCA+SCAN=0");
    (void)transmitAndExpectOkLogged(
        "scan post A2DPDISC",
        core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpDisconnect));
    (void)transmitAndExpectOkLogged("scan post DSCA",
                                  core::buildBt1035DisconnectAllLine());
    (void)transmitAndExpectOkLogged("scan post SCAN=0",
                                    core::buildBt1035StopScanLine());

    if (result) {
        ESP_LOGI(kTag, "======== BT scan session OK: %u device(s) ========",
                 static_cast<unsigned>(result->size()));
    } else {
        ESP_LOGW(kTag, "======== BT scan session FAILED ========");
    }

    return result;
}

std::expected<void, Bt1035Error> Bt1035Driver::stopScan()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return transmitAndExpectOk(core::buildBt1035StopScanLine());
}

void Bt1035Driver::prepareForOutgoingConnect()
{
    ESP_LOGI(kTag, "connect prep: LINKCFG/AUTOCONN off");
    (void)transmitAndExpectOk(core::buildBt1035DisableAutoLinkLine());
    (void)transmitAndExpectOk(core::buildBt1035SetAutoConnLine(0U));
    flushUartRx(uartPort_);
}

bool Bt1035Driver::waitForA2dpConnected(int timeoutMs)
{
    const TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
    while (xTaskGetTickCount() < deadline) {
        auto state = queryA2dpState();
        if (!state) {
            return false;
        }
        ESP_LOGI(kTag, "connect wait: A2DPSTAT=%u",
                 static_cast<unsigned>(static_cast<std::uint8_t>(*state)));
        if (*state == core::Bt1035A2dpState::Connected
            || *state == core::Bt1035A2dpState::Streaming
            || *state == core::Bt1035A2dpState::Paused) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

std::expected<void, Bt1035Error> Bt1035Driver::startA2dpAudio()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    ESP_LOGI(kTag, "A2DP audio start (AT+A2DPAUDIO=1)");
    return transmitAndExpectOk(core::buildBt1035A2dpAudioLine(true));
}

bool Bt1035Driver::waitForA2dpStreaming(int timeoutMs)
{
    const TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
    TickType_t lastAudioStartAttempt = 0;
    while (xTaskGetTickCount() < deadline) {
        auto state = queryA2dpState();
        if (state) {
            ESP_LOGI(kTag, "stream wait: A2DPSTAT=%u",
                     static_cast<unsigned>(static_cast<std::uint8_t>(*state)));
            if (*state == core::Bt1035A2dpState::Streaming) {
                return true;
            }
            const TickType_t now = xTaskGetTickCount();
            if ((*state == core::Bt1035A2dpState::Connected
                 || *state == core::Bt1035A2dpState::Paused)
                && (lastAudioStartAttempt == 0U
                    || (now - lastAudioStartAttempt) >= pdMS_TO_TICKS(3000))) {
                lastAudioStartAttempt = now;
                if (auto started = startA2dpAudio(); !started) {
                    ESP_LOGW(kTag, "A2DPAUDIO=1 failed (%d)",
                             static_cast<int>(started.error()));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    return false;
}

std::expected<void, Bt1035Error> Bt1035Driver::connectA2dp(std::string_view mac)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    prepareForOutgoingConnect();
    const std::string line = core::buildBt1035A2dpConnectLine(mac);
    if (line.empty()) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    (void)stopScan();
    auto response = transmitAndCollect(line, kConnectTimeoutMs);
    if (!response) {
        return std::unexpected(response.error());
    }
    return {};
}

std::expected<void, Bt1035Error> Bt1035Driver::runInitSequence()
{
    for (const core::Bt1035AtCommand command : core::bootInitSequence()) {
        if (auto result = transmitAndExpectOk(core::buildBt1035AtLine(command));
            !result) {
            return result;
        }
    }
    return {};
}

std::expected<void, Bt1035Error> Bt1035Driver::resetAndInitOnce()
{
    // RESET# (pin 8) is deliberately never driven by this driver (see
    // boot()'s GPIO config below): it's left a floating input, relying
    // entirely on the BT1035's own "fixed strong pull-up to VDD_IO"
    // (datasheet §4.8), which the datasheet explicitly says means the pin
    // "can therefore be left unconnected". Diagnostic test (2026-08-22) to
    // check whether the previous external RESET# drive was contributing to
    // the intermittent boot failures.
    //
    // SYS_CTRL (pin 34) alternative usage (2026-08-22 experiment): drive a
    // genuine LOW-then-HIGH cycle every time this function runs, not just
    // once ever. With RESET# now Hi-Z, SYS_CTRL is the only pin this
    // driver can still use to force a real power-cycle — previously it
    // was asserted HIGH exactly once at first boot and never touched
    // again, which meant every later retry (hardware::bt1035RetryTask
    // calls boot() again on failure) just re-listened on an
    // already-asserted line without ever actually power-cycling the
    // module. Held LOW for kSysCtlDeassertMs first so the module's own
    // Reset Protection timeout actually elapses (a shorter pulse risks
    // the module staying "protected" on, per datasheet §4.8, so the
    // retry wouldn't be a real fresh power-on at all), then HIGH for the
    // datasheet's own >=20ms minimum.
    const auto sysCtlPin = static_cast<gpio_num_t>(pins_.sysCtlGpio);
    const auto resetPin = static_cast<gpio_num_t>(pins_.resetGpio);
    const auto ctsPin = static_cast<gpio_num_t>(pins_.ctsGpio);
    const auto rtsPin = static_cast<gpio_num_t>(pins_.rtsGpio);

    gpio_set_level(sysCtlPin, 0);
    vTaskDelay(pdMS_TO_TICKS(kSysCtlDeassertMs));
    gpio_set_level(sysCtlPin, 1);
    vTaskDelay(pdMS_TO_TICKS(kSysCtlAssertMs));
    ESP_LOGI(kTag, "power-up: SYS_CTRL=%d (want 1, driven) RESET#=%d "
                   "(want 1, Hi-Z + internal pull-up, not driven by us)",
             gpio_get_level(sysCtlPin), gpio_get_level(resetPin));
    ESP_LOGI(kTag, "before banner wait: CTS=%d (module's flow-control "
                   "input, host side floating) RTS/PIO2=%d (module's "
                   "PA_MUTE by factory default)",
             gpio_get_level(ctsPin), gpio_get_level(rtsPin));

    logRawUartBoot(uartPort_);
    ESP_LOGI(kTag, "after banner wait: CTS=%d RTS/PIO2=%d",
             gpio_get_level(ctsPin), gpio_get_level(rtsPin));
    uart_flush_input(static_cast<uart_port_t>(uartPort_));
    vTaskDelay(pdMS_TO_TICKS(kPostUartMs));

    return runInitSequence();
}

std::expected<void, Bt1035Error> Bt1035Driver::boot()
{
    if (booted_) {
        return {};
    }

    // RESET# (pin 8) is never driven by this driver (2026-08-22 diagnostic
    // change): configured as a pure floating input, pull-up/pull-down both
    // explicitly disabled, so nothing on the ESP32 side influences this
    // net electrically — the BT1035's own internal RESET# pull-up (§4.8)
    // is the only thing holding it high. GPIO_MODE_INPUT (not OUTPUT) still
    // lets gpio_get_level() read it back for the diagnostic log below,
    // without ever driving it.
    gpio_config_t resetCfg = {};
    resetCfg.pin_bit_mask = 1ULL << pins_.resetGpio;
    resetCfg.mode = GPIO_MODE_INPUT;
    resetCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    resetCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    if (gpio_config(&resetCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    // SYS_CTRL (pin 34) stays actively driven (GPIO_MODE_INPUT_OUTPUT:
    // the INPUT bit is what makes gpio_get_level() read the real driven
    // level instead of a stale register, for the diagnostic log).
    gpio_config_t sysCfg = {};
    sysCfg.pin_bit_mask = 1ULL << pins_.sysCtlGpio;
    sysCfg.mode = GPIO_MODE_INPUT_OUTPUT;
    if (gpio_config(&sysCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    // CTS (module pin 15, host->module) and RTS (module pin 16/PIO2,
    // factory default function PA_MUTE per the Feasycom programming
    // guide) — diagnostic only (2026-08-22): this driver does not
    // implement UART hardware flow control (UART_HW_FLOWCTRL_DISABLE
    // below), so these are wired but otherwise unused. Configured as
    // floating inputs, pull-up/pull-down both disabled, purely to read
    // back their level for the diagnostic log — never driven. Exploring
    // whether the module's CTS input floating could be gating its own
    // UART TX (a common hardware-flow-control behavior), and whether RTS
    // toggles at all (would indicate the module's internal firmware is
    // alive even when UART TX is silent).
    gpio_config_t ctsCfg = {};
    ctsCfg.pin_bit_mask = 1ULL << pins_.ctsGpio;
    ctsCfg.mode = GPIO_MODE_INPUT;
    ctsCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    ctsCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    if (gpio_config(&ctsCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    gpio_config_t rtsCfg = {};
    rtsCfg.pin_bit_mask = 1ULL << pins_.rtsGpio;
    rtsCfg.mode = GPIO_MODE_INPUT;
    rtsCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    rtsCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    if (gpio_config(&rtsCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    if (!uartInstalled_) {
        const uart_config_t uartCfg = {
            .baud_rate = kBaudRate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
        };

        if (uart_driver_install(static_cast<uart_port_t>(uartPort_), kUartRxBuffer,
                              kUartTxBuffer, 0, nullptr, 0)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        if (uart_param_config(static_cast<uart_port_t>(uartPort_), &uartCfg)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        if (uart_set_pin(static_cast<uart_port_t>(uartPort_), pins_.uartTx,
                         pins_.uartRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        uartInstalled_ = true;
    }

    if (auto init = resetAndInitOnce(); !init) {
        ESP_LOGE(kTag, "AT init failed");
        probeBaudRates(uartPort_);
        return init;
    }

    (void)transmitAndExpectOk(core::buildBt1035DisableAutoLinkLine());
    ESP_LOGI(kTag, "auto-link disabled (AT+LINKCFG=0,0)");

    booted_ = true;
    ESP_LOGI(kTag, "I2S slave mode enabled (AT+AUXCFG=3, AT+I2SCFG=35)");
    return {};
}

} // namespace bt1035
