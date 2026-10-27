/**
 * @file    AudioPresetJson.cpp
 * @brief   JSON for the /api/audio/presets routes.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AudioPresetJson.hpp"

#include "core/JsonText.hpp"

#include <cstdio>
#include <cstdlib>

namespace core {

namespace {

/** Position right after `"key":` and any whitespace, or npos. */
[[nodiscard]] std::size_t valueStart(std::string_view json,
                                     std::string_view key)
{
    const std::string needle = "\"" + std::string(key) + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return pos;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string_view::npos) {
        return pos;
    }
    pos = json.find_first_not_of(" \t\r\n", pos + 1U);
    return pos;
}

[[nodiscard]] std::optional<unsigned long> uintField(std::string_view json,
                                                     std::string_view key)
{
    const std::size_t start = valueStart(json, key);
    if (start == std::string_view::npos || json[start] < '0'
        || json[start] > '9') {
        return std::nullopt;
    }
    return std::strtoul(json.data() + start, nullptr, 10);
}

/** Field that is a preset id or null: outer nullopt = missing/invalid. */
[[nodiscard]] std::optional<std::optional<PresetId>> idOrNullField(
    std::string_view json, std::string_view key)
{
    const std::size_t start = valueStart(json, key);
    if (start != std::string_view::npos
        && json.substr(start).starts_with("null")) {
        return std::optional<PresetId>{};
    }
    const auto raw = uintField(json, key);
    const auto id = raw ? PresetId::tryFromValue(*raw)
                        : std::expected<PresetId, ParseError>(
                              std::unexpected(ParseError::MissingField));
    if (!id) {
        return std::nullopt;
    }
    return std::optional<PresetId>{*id};
}

[[nodiscard]] std::optional<std::string> stringField(std::string_view json,
                                                     std::string_view key)
{
    const std::size_t start = valueStart(json, key);
    if (start == std::string_view::npos || json[start] != '"') {
        return std::nullopt;
    }
    std::size_t end = start + 1U;
    while (end < json.size() && json[end] != '"') {
        end += json[end] == '\\' ? 2U : 1U;
    }
    if (end >= json.size()) {
        return std::nullopt;
    }
    return unescapeJsonString(json.substr(start + 1U, end - start - 1U));
}

void appendId(std::string& out, std::optional<PresetId> id)
{
    out += id ? std::to_string(id->value()) : std::string("null");
}

void appendPresetBody(std::string& out, PresetId id,
                      const AudioPreset& preset)
{
    out += "\"id\":" + std::to_string(id.value()) + ",\"name\":\"";
    out += preset.name.value();
    out += "\",\"eq\":[";
    char num[16];
    bool first = true;
    for (const auto& band : preset.eq.bands()) {
        std::snprintf(num, sizeof(num), "%s%.1f", first ? "" : ",",
                      static_cast<double>(band.gain.value()));
        out += num;
        first = false;
    }
    out += "],\"stereo_level\":"
           + std::to_string(preset.enhancements.stereo.value())
           + ",\"bass_level\":"
           + std::to_string(preset.enhancements.bass.value()) + "}";
}

void appendFactory(std::string& out)
{
    out += "\"factory\":[";
    const auto presets = factoryPresets();
    for (std::uint8_t i = 0; i < presets.size(); ++i) {
        out += i == 0U ? "{" : ",{";
        appendPresetBody(out, *PresetId::tryFromValue(i), presets[i]);
    }
    out += "]";
}

void appendUser(std::string& out, const AudioPresetBank& bank)
{
    out += ",\"user\":[";
    for (std::uint8_t slot = 0; slot < PresetId::kUserSlots; ++slot) {
        out += slot == 0U ? "" : ",";
        const auto preset = bank.userSlot(slot);
        if (!preset) {
            out += "null";
            continue;
        }
        out += "{\"slot\":" + std::to_string(slot) + ",";
        appendPresetBody(out, PresetId::user(slot), *preset);
    }
    out += "]";
}

} // namespace

std::string serializePresetListJson(const AudioPresetBank& bank)
{
    std::string out = "{";
    appendFactory(out);
    appendUser(out, bank);
    out += ",\"defaults\":{\"radio\":";
    appendId(out, bank.sourceDefault(ActiveSource::Radio));
    out += ",\"stream\":";
    appendId(out, bank.sourceDefault(ActiveSource::Bluetooth));
    out += "},\"last\":";
    appendId(out, bank.last());
    out += "}";
    return out;
}

std::expected<PresetId, ParseError> parsePresetIdJson(std::string_view json)
{
    const auto raw = uintField(json, "id");
    if (!raw) {
        return std::unexpected(ParseError::MissingField);
    }
    return PresetId::tryFromValue(*raw);
}

std::expected<std::uint8_t, ParseError> parsePresetSlotJson(
    std::string_view json)
{
    const auto raw = uintField(json, "slot");
    if (!raw || *raw >= PresetId::kUserSlots) {
        return std::unexpected(ParseError::MissingField);
    }
    return static_cast<std::uint8_t>(*raw);
}

std::expected<PresetSaveRequest, ParseError> parsePresetSaveJson(
    std::string_view json)
{
    const auto slot = parsePresetSlotJson(json);
    const auto label = stringField(json, "name");
    if (!slot || !label) {
        return std::unexpected(ParseError::MissingField);
    }
    auto name = PresetName::tryFrom(*label);
    if (!name) {
        return std::unexpected(name.error());
    }
    return PresetSaveRequest{.slot = *slot, .name = std::move(*name)};
}

std::expected<PresetDefaultsRequest, ParseError> parsePresetDefaultsJson(
    std::string_view json)
{
    const auto radio = idOrNullField(json, "radio");
    const auto stream = idOrNullField(json, "stream");
    if (!radio || !stream) {
        return std::unexpected(ParseError::MissingField);
    }
    return PresetDefaultsRequest{.radio = *radio, .stream = *stream};
}

} // namespace core
