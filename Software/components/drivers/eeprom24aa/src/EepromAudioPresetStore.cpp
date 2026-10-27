/**
 * @file    EepromAudioPresetStore.cpp
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

#include "eeprom24aa/EepromAudioPresetStore.hpp"

namespace eeprom24aa {

EepromAudioPresetStore::EepromAudioPresetStore(Eeprom24aa& eeprom) noexcept
    : eeprom_(eeprom)
{
}

std::expected<core::AudioPresetBank, core::StoreError>
EepromAudioPresetStore::loadBank() const
{
    const auto image = eeprom_.readPresetImage();
    if (!image) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return core::decodePresetEepromImage(*image);
}

std::expected<void, core::StoreError> EepromAudioPresetStore::saveBank(
    const core::AudioPresetBank& bank)
{
    if (!eeprom_.writePresetImage(core::encodePresetEepromImage(bank))) {
        return std::unexpected(core::StoreError::IoFailed);
    }
    return {};
}

} // namespace eeprom24aa
