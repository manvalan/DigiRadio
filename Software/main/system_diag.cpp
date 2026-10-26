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

void confirmNow(void* /*arg*/)
{
    if (auto confirmed = ota::OtaService::confirmBoot(); !confirmed) {
        ESP_LOGW(kTag, "OTA rollback confirm failed");
        return;
    }
    ESP_LOGI(kTag, "running image confirmed after %lld s of probation",
             static_cast<long long>(kProbationSeconds));
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
    return core::serializeSystemDiagnosticsJson(collect());
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
    static esp_timer_handle_t timer = nullptr;
    if (timer != nullptr) {
        return;
    }
    const esp_timer_create_args_t args = {
        .callback = confirmNow,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_confirm",
        .skip_unhandled_events = false,
    };
    if (esp_timer_create(&args, &timer) != ESP_OK
        || esp_timer_start_once(timer, kProbationSeconds * 1000000) != ESP_OK) {
        // No timer, no probation: confirm now rather than leave the image
        // for the bootloader to undo at the next ordinary reset.
        ESP_LOGW(kTag, "probation timer unavailable, confirming now");
        confirmNow(nullptr);
    }
}

} // namespace system_diag
