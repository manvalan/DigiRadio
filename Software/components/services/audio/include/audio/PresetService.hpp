/**
 * @file    PresetService.hpp
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
#pragma once

#include "audio/AudioService.hpp"

#include "core/AudioPresetBank.hpp"
#include "core/IAudioPresetStore.hpp"
#include "core/IAudioSourceListener.hpp"
#include "core/StoreError.hpp"

#include <cstdint>
#include <expected>
#include <mutex>
#include <optional>

namespace audio {

/**
 * @brief    PresetService — owns the preset bank and drives AudioService.
 *
 * The bank is kept in two places: the primary store (NVS, every field) and
 * an optional backup (the board EEPROM, a compact copy of slots 0..5 that
 * survives an NVS erase or a full re-flash). load() falls back to the
 * backup and copies it back to the primary when NVS is empty. Volume and
 * input are never part of a preset.
 *
 * @dname    PresetService
 * @return   n/a (type)
 * @pubstate Owns bank_; borrows audio_, primary_, backup_. All public calls
 *           are serialised by mutex_.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class PresetService final : public core::IAudioSourceListener {
public:
    /**
     * @brief    PresetService — bind to the audio path and stores.
     *
     * @dname    PresetService
     * @param    audio    Live audio path (outlives this).
     * @param    primary  Full store (NVS); nullptr keeps presets in RAM only.
     * @param    backup   Compact copy (EEPROM); nullptr when absent.
     * @pubstate stores references; bank_ starts empty until load().
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    PresetService(AudioService& audio, core::IAudioPresetStore* primary,
                  core::IAudioPresetStore* backup) noexcept;

    /**
     * @brief    load — read the bank at boot (primary, else backup).
     *
     * @dname    load
     * @return   true when a stored bank was found in either store.
     * @pubstate replaces bank_; may write primary_ when restored from backup.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    bool load();

    /**
     * @brief    bank — snapshot of the current bank.
     *
     * @dname    bank
     * @return   Copy of bank_ (safe to use after the lock is released).
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] core::AudioPresetBank bank() const;

    /**
     * @brief    apply — recall a preset live and remember it.
     *
     * @dname    apply
     * @param    id  Factory or user preset.
     * @return   Ok; StoreError::NotFound for an empty user slot; IoFailed
     *           when the DSP or NVS write fails.
     * @pubstate updates the live profile (persisted) and bank_.last.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::expected<void, core::StoreError> apply(core::PresetId id);

    /**
     * @brief    saveCurrent — store the live EQ and enhancements in a slot.
     *
     * @dname    saveCurrent
     * @param    slot  User slot 0..PresetId::kUserSlots-1 (overwritten).
     * @param    name  Label shown in the app and web page.
     * @return   Ok, or StoreError::NotFound (bad slot) / IoFailed.
     * @pubstate writes bank_ and both stores.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::expected<void, core::StoreError> saveCurrent(
        std::uint8_t slot, core::PresetName name);

    /**
     * @brief    erase — empty a user slot.
     *
     * @dname    erase
     * @param    slot  User slot 0..PresetId::kUserSlots-1.
     * @return   Ok, or StoreError::NotFound (bad slot) / IoFailed.
     * @pubstate writes bank_ (also clears defaults/last that pointed at it)
     *           and both stores.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::expected<void, core::StoreError> erase(std::uint8_t slot);

    /**
     * @brief    setDefaults — presets recalled when an input is selected.
     *
     * @dname    setDefaults
     * @param    radio   For the tuner (FM/DAB); nullopt keeps the sound.
     * @param    stream  For web radio / phone; nullopt keeps the sound.
     * @return   Ok, or StoreError::NotFound (empty user slot) / IoFailed.
     * @pubstate writes bank_ and both stores.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::expected<void, core::StoreError> setDefaults(
        std::optional<core::PresetId> radio,
        std::optional<core::PresetId> stream);

    /**
     * @brief    onSourceSelected — recall the input's default preset.
     *
     * @dname    onSourceSelected
     * @param    source  Input MX1 just switched to (output still faded).
     * @pubstate may change the live EQ (not persisted, so the user's own
     *           profile still loads at boot before the routing runs).
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    void onSourceSelected(core::ActiveSource source) override;

private:
    [[nodiscard]] std::expected<void, core::StoreError> persist(
        bool withBackup);

    AudioService& audio_;
    core::IAudioPresetStore* primary_;
    core::IAudioPresetStore* backup_;
    core::AudioPresetBank bank_;
    mutable std::mutex mutex_;
};

} // namespace audio
