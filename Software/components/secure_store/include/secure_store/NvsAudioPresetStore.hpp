/**
 * @file    NvsAudioPresetStore.hpp
 * @brief   NVS-backed IAudioPresetStore (primary copy of the preset bank).
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

namespace secure_store {

/**
 * @brief    NvsAudioPresetStore — persists the preset bank as an NVS blob.
 *
 * @dname    NvsAudioPresetStore
 * @return   n/a (type)
 * @pubstate Uses namespace digiradio, key audio_presets; the blob format is
 *           core::encodePresetBankBlob().
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class NvsAudioPresetStore final : public core::IAudioPresetStore {
public:
    [[nodiscard]] std::expected<core::AudioPresetBank, core::StoreError>
    loadBank() const override;

    [[nodiscard]] std::expected<void, core::StoreError> saveBank(
        const core::AudioPresetBank& bank) override;
};

} // namespace secure_store
