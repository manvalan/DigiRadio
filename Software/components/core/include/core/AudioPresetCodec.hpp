/**
 * @file    AudioPresetCodec.hpp
 * @brief   Binary encodings of AudioPresetBank for NVS and the 24AA025.
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

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace core {

/** Bytes of the 24AA025 preset image (word addresses 0x10..0x7F). */
inline constexpr std::size_t kPresetEepromImageSize = 112U;
/** First EEPROM word address of the preset image (16-byte page aligned). */
inline constexpr std::uint8_t kPresetEepromBase = 0x10U;
/** User slots that fit in the EEPROM image (one 16-byte page each). */
inline constexpr std::uint8_t kPresetEepromSlots = 6U;

/** Raw EEPROM preset image. */
using PresetEepromImage = std::array<std::uint8_t, kPresetEepromImageSize>;

/**
 * @brief    encodePresetBankBlob — full-fidelity NVS blob of a bank.
 *
 * @dname    encodePresetBankBlob
 * @param    bank  Bank to serialise (all ten slots, full EQ settings).
 * @return   Versioned byte blob.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::vector<std::uint8_t> encodePresetBankBlob(
    const AudioPresetBank& bank);

/**
 * @brief    decodePresetBankBlob — parse an NVS blob back into a bank.
 *
 * @dname    decodePresetBankBlob
 * @param    blob  Bytes from encodePresetBankBlob().
 * @return   Bank, or StoreError::InvalidData on any malformed field.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<AudioPresetBank, StoreError> decodePresetBankBlob(
    std::span<const std::uint8_t> blob);

/**
 * @brief    encodePresetEepromImage — compact backup for the 24AA025.
 *
 * Page 0 holds magic, version, last and per-source defaults; pages 1..6
 * hold user slots 0..5 as a 7-byte name, six half-dB band gains and the
 * two enhancement levels. Every page ends with a CRC-8 so a torn write
 * only loses that page. Centre frequencies and Q are not stored: a slot
 * restored from EEPROM uses the factory EQ centres.
 *
 * @dname    encodePresetEepromImage
 * @param    bank  Bank to back up (slots 6..9 are not mirrored).
 * @return   112-byte image for word addresses 0x10..0x7F.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] PresetEepromImage encodePresetEepromImage(
    const AudioPresetBank& bank);

/**
 * @brief    decodePresetEepromImage — restore a bank from the EEPROM.
 *
 * @dname    decodePresetEepromImage
 * @param    image  112 bytes read from 0x10..0x7F.
 * @return   Bank (pages with a bad CRC are skipped), or
 *           StoreError::NotFound when the header page is blank/invalid.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<AudioPresetBank, StoreError>
decodePresetEepromImage(const PresetEepromImage& image);

} // namespace core
