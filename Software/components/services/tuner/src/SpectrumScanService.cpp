/**
 * @file    SpectrumScanService.cpp
 * @brief   SpectrumScanService implementation (FreeRTOS sweep task).
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
 * @date    2026-10-26
 */

#include "tuner/SpectrumScanService.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tuner {

namespace {

constexpr char kTag[] = "spectrum";

// The sweep task copies TunerStatus snapshots (labels, AF vectors) like
// af_follow does, but never hunts: 8 KB leaves margin. It exists only
// while a sweep runs.
constexpr std::uint32_t kTaskStackBytes = 8192U;
constexpr UBaseType_t kTaskPriority = 3U;

// RSQ after FM_TUNE_FREQ needs a short settle (TunerService uses 120 ms
// for its seek scan); DAB acquisition needs longer before DIGRAD
// reports CNR/FIC (TunerService DAB scan: 600 ms).
constexpr TickType_t kFmSettle = pdMS_TO_TICKS(120);
constexpr TickType_t kDabSettle = pdMS_TO_TICKS(600);

} // namespace

SpectrumScanService::SpectrumScanService(TunerService& tuner)
    : tuner_(tuner)
{
}

std::expected<void, core::SpectrumStartError> SpectrumScanService::start(
    core::TunerBand band)
{
    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.state == core::SpectrumState::Running) {
        return std::unexpected(core::SpectrumStartError::Busy);
    }
    const core::SpectrumSnapshot previous = std::move(snapshot_);
    snapshot_ = core::SpectrumSnapshot{};
    snapshot_.state = core::SpectrumState::Running;
    snapshot_.band = band;
    if (xTaskCreate(taskEntry, "spectrum", kTaskStackBytes, this,
                    kTaskPriority, nullptr)
        != pdPASS) {
        snapshot_ = previous;
        return std::unexpected(core::SpectrumStartError::NoResource);
    }
    return {};
}

std::string SpectrumScanService::json()
{
    const std::lock_guard<std::mutex> lock(mutex_);
    return core::serializeSpectrumJson(snapshot_);
}

void SpectrumScanService::taskEntry(void* arg)
{
    static_cast<SpectrumScanService*>(arg)->run();
    vTaskDelete(nullptr);
}

void SpectrumScanService::run()
{
    tuner_.setSweepActive(true);
    const std::optional<Origin> origin = captureOrigin();
    if (!tuner_.setVolume(0U)) {
        ESP_LOGW(kTag, "mute failed: the sweep will be audible");
    }
    bool fm = true;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        fm = snapshot_.band == core::TunerBand::Fm;
    }
    const bool ok = fm ? sweepFm() : sweepDab();
    if (origin) {
        restore(*origin);
    }
    tuner_.setSweepActive(false);
    finish(ok);
    ESP_LOGI(kTag, "%s sweep %s", fm ? "FM" : "DAB", ok ? "done" : "failed");
}

bool SpectrumScanService::sweepFm()
{
    const std::size_t total = core::spectrumFmPointCount();
    for (std::size_t i = 0U; i < total; ++i) {
        const std::uint32_t khz =
            core::kSpectrumFmStartKhz
            + static_cast<std::uint32_t>(i) * core::kSpectrumFmStepKhz;
        const auto frequency = core::FrequencyKHz::tryFromKhz(khz);
        if (!frequency || !tuner_.tuneFm(*frequency)) {
            return false;
        }
        vTaskDelay(kFmSettle);
        const auto status = tuner_.refreshSignal();
        if (!status) {
            return false;
        }
        core::SpectrumPoint point;
        point.freqKhz = khz;
        point.rssiDbuV = status->fmRssiDbuV;
        point.snrDb = status->fmSnrDb;
        record(point, total);
    }
    return true;
}

bool SpectrumScanService::sweepDab()
{
    const std::size_t total = core::kDabBand3FrequenciesKhz.size();
    for (std::size_t i = 0U; i < total; ++i) {
        const auto index = static_cast<std::uint8_t>(i);
        if (!tuner_.tuneDab(index)) {
            return false;
        }
        vTaskDelay(kDabSettle);
        const auto status = tuner_.refreshSignal();
        if (!status) {
            return false;
        }
        core::SpectrumPoint point;
        point.freqKhz = core::kDabBand3FrequenciesKhz[i];
        point.rssiDbuV = status->dabRssiDbuV;
        point.dabChannel = index;
        if (status->locked) {
            point.cnrDb = status->dabCnrDb;
            point.ficQuality = status->dabFicQuality;
        }
        record(point, total);
    }
    return true;
}

void SpectrumScanService::record(const core::SpectrumPoint& point,
                                 std::size_t total)
{
    const std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.points.empty()) {
        snapshot_.points.reserve(total);
    }
    snapshot_.points.push_back(point);
    snapshot_.progress = core::spectrumProgress(snapshot_.points.size(), total);
}

void SpectrumScanService::finish(bool ok)
{
    const std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.state = ok ? core::SpectrumState::Done : core::SpectrumState::Failed;
    if (ok) {
        snapshot_.progress = 100U;
    }
}

std::optional<SpectrumScanService::Origin> SpectrumScanService::captureOrigin()
{
    const auto status = tuner_.refreshStatus();
    if (!status) {
        ESP_LOGW(kTag, "no tuner status: station will not be restored");
        return std::nullopt;
    }
    return Origin{.band = status->band,
                  .fmFrequency = status->fmFrequency,
                  .dabIndex = status->dabFreqIndex.value_or(0U),
                  .serviceId = status->dabPlayingServiceId,
                  .componentId = status->dabPlayingComponentId,
                  .volume = status->volume};
}

void SpectrumScanService::restore(const Origin& origin)
{
    bool ok = true;
    if (origin.band == core::TunerBand::Fm && origin.fmFrequency) {
        ok = tuner_.tuneFm(*origin.fmFrequency).has_value();
    } else if (origin.band == core::TunerBand::Dab) {
        ok = tuner_.tuneDab(origin.dabIndex).has_value();
        if (ok && origin.serviceId && origin.componentId) {
            vTaskDelay(kDabSettle);
            ok = tuner_.playDabService(*origin.serviceId, *origin.componentId)
                     .has_value();
        }
    }
    ok = tuner_.setVolume(origin.volume).has_value() && ok;
    if (!ok) {
        ESP_LOGW(kTag, "previous station/volume not fully restored");
    }
}

} // namespace tuner
