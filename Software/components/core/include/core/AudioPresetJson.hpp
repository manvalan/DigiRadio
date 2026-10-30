/**
 * @file    AudioPresetJson.hpp
 * @brief   JSON for /api/audio/presets (list, apply, save, delete, defaults).
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
#include "core/ParseError.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace core {

/** Body of POST /api/audio/presets/save. */
struct PresetSaveRequest {
    std::uint8_t slot; ///< 0..PresetId::kUserSlots-1.
    PresetName name;   ///< Label for the new preset.
};

/** Body of POST /api/audio/presets/defaults (nullopt = keep sound). */
struct PresetDefaultsRequest {
    std::optional<PresetId> radio;  ///< Recalled when the tuner is selected.
    std::optional<PresetId> stream; ///< Recalled for web radio / phone.
};

/**
 * @brief    serializePresetListJson — GET /api/audio/presets body.
 *
 * {"factory":[{"id","name","eq":[6 gains],"stereo_level","bass_level"}],
 *  "user":[{"slot","id","name",...}|null x10],
 *  "defaults":{"radio":id|null,"stream":id|null},"last":id|null}
 *
 * @dname    serializePresetListJson
 * @param    bank  Current preset bank.
 * @return   JSON text.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::string serializePresetListJson(const AudioPresetBank& bank);

/**
 * @brief    parsePresetIdJson — {"id":N} for apply.
 *
 * @dname    parsePresetIdJson
 * @param    json  Request body.
 * @return   PresetId, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<PresetId, ParseError> parsePresetIdJson(
    std::string_view json);

/**
 * @brief    parsePresetSlotJson — {"slot":N} for delete.
 *
 * @dname    parsePresetSlotJson
 * @param    json  Request body.
 * @return   Slot 0..9, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<std::uint8_t, ParseError> parsePresetSlotJson(
    std::string_view json);

/**
 * @brief    parsePresetSaveJson — {"slot":N,"name":"..."} for save.
 *
 * @dname    parsePresetSaveJson
 * @param    json  Request body.
 * @return   PresetSaveRequest, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<PresetSaveRequest, ParseError>
parsePresetSaveJson(std::string_view json);

/**
 * @brief    parsePresetDefaultsJson — {"radio":N|null,"stream":N|null}.
 *
 * @dname    parsePresetDefaultsJson
 * @param    json  Request body (both keys required).
 * @return   PresetDefaultsRequest, or ParseError::MissingField.
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::expected<PresetDefaultsRequest, ParseError>
parsePresetDefaultsJson(std::string_view json);

} // namespace core
