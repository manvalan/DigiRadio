/**
 * @file    NvsAudioProfileStore.hpp
 * @brief   NVS-backed IAudioProfileStore for user audio settings.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/IAudioProfileStore.hpp"

namespace secure_store {

/**
 * @brief    NvsAudioProfileStore — persists AudioProfile JSON in NVS.
 *
 * @dname    NvsAudioProfileStore
 * @return   n/a (type)
 * @pubstate Uses namespace digiradio, key audio_profile_json.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class NvsAudioProfileStore final : public core::IAudioProfileStore {
public:
    [[nodiscard]] bool hasProfile() const override;

    [[nodiscard]] std::expected<void, core::StoreError> saveProfile(
        const core::AudioProfile& profile) override;

    [[nodiscard]] std::expected<core::AudioProfile, core::StoreError>
    loadProfile() const override;

    [[nodiscard]] std::expected<void, core::StoreError> clearProfile() override;
};

} // namespace secure_store
