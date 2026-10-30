/**
 * @file    EepromAudioPresetStore.hpp
 * @brief   IAudioPresetStore backed by the 24AA025E48 (preset backup copy).
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

#include "core/IAudioPresetStore.hpp"
#include "eeprom24aa/Eeprom24aa.hpp"

namespace eeprom24aa {

/**
 * @brief    EepromAudioPresetStore — compact preset copy that survives an
 *           NVS erase or a full flash of the ESP32.
 *
 * @dname    EepromAudioPresetStore
 * @return   n/a (type)
 * @pubstate Borrows an Eeprom24aa. Keeps user slots 0..5 with 7-character
 *           names, 0.5 dB gain steps and per-input defaults
 *           (core::encodePresetEepromImage()); NVS stays the full copy.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class EepromAudioPresetStore final : public core::IAudioPresetStore {
public:
    /**
     * @brief    EepromAudioPresetStore — bind to the board EEPROM.
     *
     * @dname    EepromAudioPresetStore
     * @param    eeprom  Driver that outlives this store.
     * @pubstate stores a reference only.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    explicit EepromAudioPresetStore(Eeprom24aa& eeprom) noexcept;

    [[nodiscard]] std::expected<core::AudioPresetBank, core::StoreError>
    loadBank() const override;

    [[nodiscard]] std::expected<void, core::StoreError> saveBank(
        const core::AudioPresetBank& bank) override;

private:
    Eeprom24aa& eeprom_;
};

} // namespace eeprom24aa
