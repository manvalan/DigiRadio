/**
 * @file    PresetService.cpp
 * @brief   Audio presets: recall, save, delete, per-input defaults.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "audio/PresetService.hpp"

#include "esp_log.h"

namespace audio {

namespace {
constexpr char kTag[] = "PresetService";
} // namespace

PresetService::PresetService(AudioService& audio,
                             core::IAudioPresetStore* primary,
                             core::IAudioPresetStore* backup) noexcept
    : audio_(audio)
    , primary_(primary)
    , backup_(backup)
{
}

bool PresetService::load()
{
    const std::lock_guard lock(mutex_);
    if (primary_ != nullptr) {
        if (auto stored = primary_->loadBank(); stored) {
            bank_ = *stored;
            ESP_LOGI(kTag, "presets loaded from NVS");
            return true;
        }
    }
    if (backup_ != nullptr) {
        if (auto stored = backup_->loadBank(); stored) {
            bank_ = *stored;
            ESP_LOGI(kTag, "presets restored from EEPROM backup");
            if (primary_ != nullptr && !primary_->saveBank(bank_)) {
                ESP_LOGW(kTag, "could not copy EEPROM presets to NVS");
            }
            return true;
        }
    }
    ESP_LOGI(kTag, "no saved presets, factory set only");
    return false;
}

core::AudioPresetBank PresetService::bank() const
{
    const std::lock_guard lock(mutex_);
    return bank_;
}

std::expected<void, core::StoreError> PresetService::apply(core::PresetId id)
{
    const std::lock_guard lock(mutex_);
    const auto preset = bank_.find(id);
    if (!preset) {
        return std::unexpected(core::StoreError::NotFound);
    }
    if (auto applied = audio_.applyPreset(*preset, true); !applied) {
        return applied;
    }
    bank_.setLast(id);
    // "last" only matters for the UI: NVS is enough, spare the EEPROM.
    return persist(false);
}

std::expected<void, core::StoreError> PresetService::saveCurrent(
    std::uint8_t slot, core::PresetName name)
{
    if (slot >= core::PresetId::kUserSlots) {
        return std::unexpected(core::StoreError::NotFound);
    }
    const std::lock_guard lock(mutex_);
    bank_.store(slot, core::AudioPreset::fromProfile(std::move(name),
                                                     audio_.currentProfile()));
    bank_.setLast(core::PresetId::user(slot));
    return persist(true);
}

std::expected<void, core::StoreError> PresetService::erase(std::uint8_t slot)
{
    if (slot >= core::PresetId::kUserSlots) {
        return std::unexpected(core::StoreError::NotFound);
    }
    const std::lock_guard lock(mutex_);
    bank_.erase(slot);
    return persist(true);
}

std::expected<void, core::StoreError> PresetService::setDefaults(
    std::optional<core::PresetId> radio, std::optional<core::PresetId> stream)
{
    const std::lock_guard lock(mutex_);
    if ((radio && !bank_.find(*radio)) || (stream && !bank_.find(*stream))) {
        return std::unexpected(core::StoreError::NotFound);
    }
    bank_.setSourceDefault(core::ActiveSource::Radio, radio);
    bank_.setSourceDefault(core::ActiveSource::Bluetooth, stream);
    return persist(true);
}

void PresetService::onSourceSelected(core::ActiveSource source)
{
    const std::lock_guard lock(mutex_);
    const auto id = bank_.sourceDefault(source);
    if (!id) {
        return;
    }
    const auto preset = bank_.find(*id);
    if (!preset) {
        return;
    }
    if (!audio_.applyPreset(*preset, false)) {
        ESP_LOGW(kTag, "default preset %u for input not applied",
                 static_cast<unsigned>(id->value()));
    }
}

std::expected<void, core::StoreError> PresetService::persist(bool withBackup)
{
    if (withBackup && backup_ != nullptr && !backup_->saveBank(bank_)) {
        // The backup is best effort; NVS below is the copy that counts.
        ESP_LOGW(kTag, "EEPROM preset backup failed");
    }
    if (primary_ == nullptr) {
        return {};
    }
    return primary_->saveBank(bank_);
}

} // namespace audio
