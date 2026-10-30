/**
 * @file    IAudioPresetStore.hpp
 * @brief   Persistence interface for the audio preset bank.
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

#include "core/AudioPresetBank.hpp"
#include "core/StoreError.hpp"

#include <expected>

namespace core {

/**
 * @brief    IAudioPresetStore — load/save the preset bank (NVS or EEPROM).
 *
 * @dname    IAudioPresetStore
 * @return   n/a (interface)
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class IAudioPresetStore {
public:
    virtual ~IAudioPresetStore() = default;

    /**
     * @brief    loadBank — read the stored bank.
     *
     * @dname    loadBank
     * @return   Bank, or StoreError::NotFound / IoFailed / InvalidData.
     * @pubstate reads backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] virtual std::expected<AudioPresetBank, StoreError> loadBank()
        const = 0;

    /**
     * @brief    saveBank — persist the bank.
     *
     * @dname    saveBank
     * @param    bank  Bank to write.
     * @return   Ok, or StoreError::IoFailed.
     * @pubstate writes backing storage via implementation.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] virtual std::expected<void, StoreError> saveBank(
        const AudioPresetBank& bank) = 0;
};

} // namespace core
