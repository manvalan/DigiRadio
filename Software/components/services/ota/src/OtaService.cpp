/**
 * @file    OtaService.cpp
 * @brief   OtaService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "ota/OtaService.hpp"

#include "core/OtaAppDescriptor.hpp"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include <algorithm>

namespace ota {

namespace {

constexpr char kTag[] = "OtaService";
constexpr int kMaxOtaImageSize = 0x1B0000;
constexpr std::size_t kHeaderCaptureMax = 512U;

void captureHeaderPrefix(std::vector<std::uint8_t>& prefix,
                         std::span<const std::uint8_t> chunk)
{
    if (prefix.size() >= kHeaderCaptureMax) {
        return;
    }
    const std::size_t remaining = kHeaderCaptureMax - prefix.size();
    const std::size_t take = std::min(remaining, chunk.size());
    prefix.insert(prefix.end(), chunk.begin(), chunk.begin() + take);
}

} // namespace

std::expected<void, OtaError> OtaService::confirmBoot()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == nullptr) {
        ESP_LOGE(kTag, "running partition unavailable");
        return std::unexpected(OtaError::ConfirmFailed);
    }

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (esp_ota_get_state_partition(running, &state) != ESP_OK) {
        ESP_LOGE(kTag, "partition state read failed");
        return std::unexpected(OtaError::ConfirmFailed);
    }

    if (state != ESP_OTA_IMG_PENDING_VERIFY) {
        return {};
    }

    if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) {
        ESP_LOGE(kTag, "mark_app_valid failed");
        return std::unexpected(OtaError::ConfirmFailed);
    }

    ESP_LOGI(kTag, "OTA image confirmed — rollback cancelled");
    return {};
}

std::expected<void, OtaError> OtaService::beginStream(int contentLength)
{
    if (active_) {
        return std::unexpected(OtaError::SessionActive);
    }
    if (contentLength <= 0 || contentLength > kMaxOtaImageSize) {
        return std::unexpected(OtaError::ImageTooLarge);
    }

    const esp_partition_t* updatePartition =
        esp_ota_get_next_update_partition(nullptr);
    if (updatePartition == nullptr) {
        return std::unexpected(OtaError::NoUpdatePartition);
    }

    esp_ota_handle_t handle = 0;
    if (esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &handle)
        != ESP_OK) {
        return std::unexpected(OtaError::BeginFailed);
    }

    active_ = true;
    descriptorValidated_ = false;
    expectedSize_ = contentLength;
    bytesWritten_ = 0;
    otaHandle_ = reinterpret_cast<void*>(static_cast<std::uintptr_t>(handle));
    updatePartition_ = updatePartition;
    headerPrefix_.clear();
    lastImageError_ = core::OtaImageError::InsufficientHeader;
    return {};
}

std::expected<void, OtaError> OtaService::writeChunk(
    std::span<const std::uint8_t> chunk)
{
    if (!active_ || otaHandle_ == nullptr) {
        return std::unexpected(OtaError::BeginFailed);
    }

    if (bytesWritten_ + static_cast<int>(chunk.size()) > expectedSize_) {
        abort();
        return std::unexpected(OtaError::ImageTooLarge);
    }

    if (!descriptorValidated_) {
        captureHeaderPrefix(headerPrefix_, chunk);
        const auto validated = core::validateOtaAppDescriptor(headerPrefix_);
        if (validated) {
            descriptorValidated_ = true;
        } else {
            lastImageError_ = validated.error();
            if (validated.error() != core::OtaImageError::InsufficientHeader
                || headerPrefix_.size() >= kHeaderCaptureMax) {
                abort();
                return std::unexpected(OtaError::WriteFailed);
            }
        }
    }

    const auto handle =
        static_cast<esp_ota_handle_t>(
            reinterpret_cast<std::uintptr_t>(otaHandle_));
    if (esp_ota_write(handle, chunk.data(), chunk.size()) != ESP_OK) {
        abort();
        return std::unexpected(OtaError::WriteFailed);
    }

    bytesWritten_ += static_cast<int>(chunk.size());
    return {};
}

std::expected<void, OtaError> OtaService::finishStream()
{
    if (!active_ || otaHandle_ == nullptr || updatePartition_ == nullptr) {
        return std::unexpected(OtaError::BeginFailed);
    }
    if (bytesWritten_ != expectedSize_) {
        abort();
        return std::unexpected(OtaError::WriteFailed);
    }
    if (!descriptorValidated_) {
        lastImageError_ = core::OtaImageError::InsufficientHeader;
        abort();
        return std::unexpected(OtaError::WriteFailed);
    }

    const auto handle =
        static_cast<esp_ota_handle_t>(
            reinterpret_cast<std::uintptr_t>(otaHandle_));
    if (esp_ota_end(handle) != ESP_OK) {
        resetSession();
        return std::unexpected(OtaError::EndFailed);
    }

    const auto* partition =
        static_cast<const esp_partition_t*>(updatePartition_);
    if (esp_ota_set_boot_partition(partition) != ESP_OK) {
        resetSession();
        return std::unexpected(OtaError::SetBootFailed);
    }

    resetSession();
    return {};
}

void OtaService::abort() noexcept
{
    if (active_ && otaHandle_ != nullptr) {
        const auto handle =
            static_cast<esp_ota_handle_t>(
                reinterpret_cast<std::uintptr_t>(otaHandle_));
        esp_ota_abort(handle);
    }
    resetSession();
}

core::OtaImageError OtaService::lastImageError() const noexcept
{
    return lastImageError_;
}

void OtaService::resetSession() noexcept
{
    active_ = false;
    descriptorValidated_ = false;
    expectedSize_ = 0;
    bytesWritten_ = 0;
    otaHandle_ = nullptr;
    updatePartition_ = nullptr;
    headerPrefix_.clear();
}

} // namespace ota
