/**
 * @file    system_diag.cpp
 * @brief   Runtime diagnostics (reset reason, heap, task stacks) and the
 *          delayed OTA confirmation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "system_diag.hpp"

#include "hardware_bootstrap.hpp"
#include "sdkconfig.h"

#include "core/SystemDiagnostics.hpp"
#include "ota/OtaService.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"

#include <array>
#include <atomic>
#include <string_view>

namespace system_diag {

namespace {

constexpr char kTag[] = "system_diag";
constexpr std::size_t kMaxTasks = 12U;
/** Long enough for every task to have started and for the first tune,
 *  stream and AF check to have run (af_follow polls every 3 s). */
constexpr std::int64_t kProbationSeconds = 60;
/** How often the probation check runs once kProbationSeconds passed. */
constexpr std::int64_t kProbationCheckUs = 5LL * 1000000LL;
/** heap_min_free below this means the image nearly ran out of memory
 *  (0.12.3 hit 1.6 KB and lost Wi-Fi): leave it unconfirmed. */
constexpr std::size_t kMinHealthyHeapBytes = 4096U;
/** Web radio autostart waits at least this long after boot. */
constexpr std::int64_t kSettleSeconds = 20;

/** A client read GET /api/health after the probation time: Wi-Fi, lwIP
 *  and the web server demonstrably work. */
std::atomic<bool> gHealthSeenAfterProbation{false};
esp_timer_handle_t gProbationTimer = nullptr;

struct WatchedTask {
    const char* name;
    TaskHandle_t handle;
};

std::array<WatchedTask, kMaxTasks> gTasks{};
std::atomic<std::size_t> gTaskCount{0U};

[[nodiscard]] std::string_view resetReasonToken(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON:   return "power_on";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:  return "task_watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_EXT:       return "external";
    default:                return "other";
    }
}

[[nodiscard]] bool pendingVerify(const esp_partition_t* running)
{
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    return running != nullptr
           && esp_ota_get_state_partition(running, &state) == ESP_OK
           && state == ESP_OTA_IMG_PENDING_VERIFY;
}

[[nodiscard]] core::SystemDiagnostics collect()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    core::SystemDiagnostics diag;
    diag.resetReason = resetReasonToken(esp_reset_reason());
    diag.uptimeSeconds =
        static_cast<std::uint32_t>(esp_timer_get_time() / 1000000);
    diag.heapFreeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    diag.heapMinFreeBytes =
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    diag.heapLargestBlockBytes =
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    diag.runningPartition = running != nullptr ? running->label : "unknown";
    diag.otaPendingVerify = pendingVerify(running);
    diag.si4684Store = hardware::HardwareBootstrap::imageStoreState();
#if CONFIG_SI4684_EMBEDDED_IMAGES
    diag.si4684ImagesEmbedded = true;
#else
    diag.si4684ImagesEmbedded = false;
#endif
    const std::size_t count = gTaskCount.load();
    for (std::size_t i = 0U; i < count; ++i) {
        diag.tasks.push_back(core::TaskStackReport{
            .name = gTasks[i].name,
            .minFreeBytes = static_cast<std::uint32_t>(
                uxTaskGetStackHighWaterMark(gTasks[i].handle)),
        });
    }
    return diag;
}

void confirmNow()
{
    if (auto confirmed = ota::OtaService::confirmBoot(); !confirmed) {
        ESP_LOGW(kTag, "OTA rollback confirm failed");
        return;
    }
    ESP_LOGI(kTag, "running image confirmed after %lld s",
             static_cast<long long>(esp_timer_get_time() / 1000000));
}

// Periodic esp_timer callback: confirm a pending image once it has run
// kProbationSeconds AND proved healthy (a health request was served after
// that, so Wi-Fi and HTTP work, and the heap never nearly ran out). A
// hung image is never confirmed: the next reset rolls it back.
void probationCheck(void* /*arg*/)
{
    esp_timer_handle_t timer = gProbationTimer;
    if (!pendingVerify(esp_ota_get_running_partition())) {
        (void)esp_timer_stop(timer);
        return;
    }
    const bool heapOk = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)
                        >= kMinHealthyHeapBytes;
    if (!gHealthSeenAfterProbation.load()) {
        return;
    }
    if (!heapOk) {
        ESP_LOGW(kTag, "heap went below %u B: image left unconfirmed",
                 static_cast<unsigned>(kMinHealthyHeapBytes));
        (void)esp_timer_stop(timer);
        return;
    }
    confirmNow();
    (void)esp_timer_stop(timer);
}

} // namespace

void watchTask(const char* name, TaskHandle_t handle)
{
    const std::size_t index = gTaskCount.load();
    if (handle == nullptr || index >= kMaxTasks) {
        return;
    }
    gTasks[index] = WatchedTask{.name = name, .handle = handle};
    gTaskCount.store(index + 1U);
}

std::string json()
{
    if (esp_timer_get_time() / 1000000 >= kProbationSeconds) {
        gHealthSeenAfterProbation.store(true);
    }
    return core::serializeSystemDiagnosticsJson(collect());
}

bool bootSettled()
{
    return esp_timer_get_time() / 1000000 >= kSettleSeconds
           && !pendingVerify(esp_ota_get_running_partition());
}

void logSummary()
{
    const core::SystemDiagnostics diag = collect();
    ESP_LOGI(kTag, "up %lus reset=%.*s heap free=%lu min=%lu largest=%lu",
             static_cast<unsigned long>(diag.uptimeSeconds),
             static_cast<int>(diag.resetReason.size()),
             diag.resetReason.data(),
             static_cast<unsigned long>(diag.heapFreeBytes),
             static_cast<unsigned long>(diag.heapMinFreeBytes),
             static_cast<unsigned long>(diag.heapLargestBlockBytes));
    for (const core::TaskStackReport& task : diag.tasks) {
        ESP_LOGI(kTag, "  stack %s min free %lu", task.name.c_str(),
                 static_cast<unsigned long>(task.minFreeBytes));
    }
}

void confirmOtaAfterProbation()
{
    if (gProbationTimer != nullptr) {
        return;
    }
    const esp_timer_create_args_t args = {
        .callback = probationCheck,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_confirm",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &gProbationTimer) != ESP_OK
        || esp_timer_start_periodic(gProbationTimer, kProbationCheckUs)
               != ESP_OK) {
        // No timer, no probation: confirm now rather than leave the image
        // for the bootloader to undo at the next ordinary reset.
        ESP_LOGW(kTag, "probation timer unavailable, confirming now");
        confirmNow();
    }
}

} // namespace system_diag
