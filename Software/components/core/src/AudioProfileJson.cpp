/**
 * @file    AudioProfileJson.cpp
 * @brief   AudioProfileJson implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/AudioProfileJson.hpp"

#include "core/EnhanceLevel.hpp"

#include <cstdlib>
#include <sstream>

namespace core {

namespace {

[[nodiscard]] bool extractJsonFloat(std::string_view json,
                                    std::string_view key,
                                    float& out)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return false;
    }
    const std::size_t valueStart = start + needle.size();
    char* end = nullptr;
    out = std::strtof(json.data() + valueStart, &end);
    return end != json.data() + valueStart;
}

[[nodiscard]] bool extractJsonUint(std::string_view json,
                                   std::string_view key,
                                   unsigned long& out)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t start = json.find(needle);
    if (start == std::string_view::npos) {
        return false;
    }
    const std::size_t valueStart = start + needle.size();
    char* end = nullptr;
    out = std::strtoul(json.data() + valueStart, &end, 10);
    return end != json.data() + valueStart;
}

[[nodiscard]] bool extractJsonBool(std::string_view json,
                                   std::string_view key,
                                   bool& out)
{
    const std::string needle =
        std::string("\"") + std::string(key) + "\":";
    const std::size_t needlePos = json.find(needle);
    if (needlePos == std::string_view::npos) {
        return false;
    }
    std::size_t start = needlePos + needle.size();
    while (start < json.size()
           && (json[start] == ' ' || json[start] == '\t'
               || json[start] == '\r' || json[start] == '\n')) {
        ++start;
    }
    const std::string_view rest = json.substr(start);
    if (rest.starts_with("true")) {
        out = true;
        return true;
    }
    if (rest.starts_with("false")) {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] std::expected<GainDb, ParseError> parseGainField(
    std::string_view json, std::string_view key)
{
    float db = 0.0F;
    if (!extractJsonFloat(json, key, db)) {
        return std::unexpected(ParseError::MissingField);
    }
    return GainDb::tryFromDb(db);
}

[[nodiscard]] std::expected<MixerState, ParseError> parseMixerJson(
    std::string_view json)
{
    const std::size_t mixerStart = json.find("\"mixer\"");
    if (mixerStart == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::string_view mixer = json.substr(mixerStart);

    const auto si4684Left = parseGainField(mixer, "si4684_left_db");
    const auto si4684Right = parseGainField(mixer, "si4684_right_db");
    const auto esp32Left = parseGainField(mixer, "esp32_left_db");
    const auto esp32Right = parseGainField(mixer, "esp32_right_db");
    const auto mixLeft = parseGainField(mixer, "mix_left_db");
    const auto mixRight = parseGainField(mixer, "mix_right_db");

    if (!si4684Left || !si4684Right || !esp32Left || !esp32Right || !mixLeft
        || !mixRight) {
        return std::unexpected(ParseError::MissingField);
    }

    return MixerState{
        .si4684Left = *si4684Left,
        .si4684Right = *si4684Right,
        .esp32Left = *esp32Left,
        .esp32Right = *esp32Right,
        .mixLeft = *mixLeft,
        .mixRight = *mixRight,
    };
}

[[nodiscard]] std::expected<EqProfile, ParseError> parseEqJson(
    std::string_view json)
{
    EqProfile profile = EqProfile::factoryDefault();
    const std::size_t eqStart = json.find("\"eq\"");
    if (eqStart == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }

    std::string_view rest = json.substr(eqStart);
    for (std::uint8_t band = 0; band < EqBandIndex::kBandCount; ++band) {
        const std::size_t objStart = rest.find('{');
        if (objStart == std::string_view::npos) {
            return std::unexpected(ParseError::MissingField);
        }
        const std::size_t objEnd = rest.find('}', objStart);
        if (objEnd == std::string_view::npos) {
            return std::unexpected(ParseError::MissingField);
        }
        const std::string_view obj = rest.substr(objStart, objEnd - objStart + 1U);

        float gainDb = 0.0F;
        unsigned long centerHz = 0U;
        float q = 0.0F;
        if (!extractJsonFloat(obj, "gain_db", gainDb)
            || !extractJsonUint(obj, "center_hz", centerHz)
            || !extractJsonFloat(obj, "q", q)) {
            return std::unexpected(ParseError::MissingField);
        }
        if (q < 0.2F || q > 10.0F) {
            return std::unexpected(ParseError::MissingField);
        }

        const auto gain = GainDb::tryFromDb(gainDb);
        const auto center = FrequencyHz::tryFromHz(
            static_cast<std::uint32_t>(centerHz));
        const auto index = EqBandIndex::tryFromIndex(band);
        if (!gain || !center || !index) {
            return std::unexpected(ParseError::MissingField);
        }

        profile.setBand(*index, EqBandSettings{
                                   .gain = *gain,
                                   .center = *center,
                                   .q = q,
                               });
        rest = rest.substr(objEnd + 1U);
    }
    return profile;
}

[[nodiscard]] std::expected<AudioEnhancements, ParseError> parseEnhancementsJson(
    std::string_view json)
{
    const std::size_t enhStart = json.find("\"enhancements\"");
    if (enhStart == std::string_view::npos) {
        return AudioEnhancements::factoryDefault();
    }

    const std::string_view enh = json.substr(enhStart);
    unsigned long stereoLevel = 0U;
    unsigned long bassLevel = 0U;
    if (!extractJsonUint(enh, "stereo_level", stereoLevel)
        || !extractJsonUint(enh, "bass_level", bassLevel)) {
        return std::unexpected(ParseError::MissingField);
    }

    const auto stereo = EnhanceLevel::tryFromLevel(stereoLevel);
    const auto bass = EnhanceLevel::tryFromLevel(bassLevel);
    if (!stereo || !bass) {
        return std::unexpected(ParseError::MissingField);
    }

    return AudioEnhancements{
        .stereo = *stereo,
        .bass = *bass,
    };
}

} // namespace

std::string serializeAudioProfileJson(const AudioProfile& profile)
{
    std::ostringstream out;
    const auto& m = profile.mixer;
    out << "{\"mixer\":{"
        << "\"si4684_left_db\":" << m.si4684Left.value() << ','
        << "\"si4684_right_db\":" << m.si4684Right.value() << ','
        << "\"esp32_left_db\":" << m.esp32Left.value() << ','
        << "\"esp32_right_db\":" << m.esp32Right.value() << ','
        << "\"mix_left_db\":" << m.mixLeft.value() << ','
        << "\"mix_right_db\":" << m.mixRight.value() << "},"
        << "\"master\":{"
        << "\"left_db\":" << profile.masterLeft.value() << ','
        << "\"right_db\":" << profile.masterRight.value() << "},"
        << "\"eq\":[";

    // Per-band "locked" flag, added 2026-08-24: band 0 is a fixed high-pass
    // Adau1701Driver::applyEq() never safeloads (SigmaStudio band 1, ST0 in
    // the compiled program -- whatever gain_db/center_hz/q is stored/sent
    // for it has zero audible effect, always). Bands 1-2 are overwritten by
    // core::applyEnhancementsToEq() with formula-derived values whenever
    // bass_level > 0; bands 3-5 likewise whenever stereo_level > 0 -- a
    // manual edit to those bands is silently inaudible while the
    // corresponding enhancement is active. This was previously
    // undiscoverable from the API response (GET echoed back the stored,
    // inert value with no indication it wasn't what was actually playing);
    // "locked":true now tells a client to grey out that slider instead of
    // letting the user "fix" a value that can't take effect.
    const bool bassLocks = profile.enhancements.bass.value() > 0U;
    const bool stereoLocks = profile.enhancements.stereo.value() > 0U;
    const bool locked[EqBandIndex::kBandCount] = {
        true, bassLocks, bassLocks, stereoLocks, stereoLocks, stereoLocks,
    };

    const auto& bands = profile.eq.bands();
    for (std::size_t i = 0; i < bands.size(); ++i) {
        if (i > 0U) {
            out << ',';
        }
        const auto& b = bands[i];
        out << "{\"gain_db\":" << b.gain.value()
            << ",\"center_hz\":" << b.center.value() << ",\"q\":" << b.q
            << ",\"locked\":" << (locked[i] ? "true" : "false")
            << '}';
    }
    out << "],\"enhancements\":{"
        << "\"stereo_level\":" << static_cast<unsigned>(profile.enhancements.stereo.value())
        << ",\"bass_level\":" << static_cast<unsigned>(profile.enhancements.bass.value())
        << "}}";
    return out.str();
}

std::expected<AudioProfile, ParseError> parseAudioProfileJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    const auto mixer = parseMixerJson(json);
    if (!mixer) {
        return std::unexpected(mixer.error());
    }

    const auto eq = parseEqJson(json);
    if (!eq) {
        return std::unexpected(eq.error());
    }

    const std::size_t masterStart = json.find("\"master\"");
    if (masterStart == std::string_view::npos) {
        return std::unexpected(ParseError::MissingField);
    }
    const std::string_view master = json.substr(masterStart);
    const auto masterLeft = parseGainField(master, "left_db");
    const auto masterRight = parseGainField(master, "right_db");
    if (!masterLeft || !masterRight) {
        return std::unexpected(ParseError::MissingField);
    }

    const auto enhancements = parseEnhancementsJson(json);
    if (!enhancements) {
        return std::unexpected(enhancements.error());
    }

    return AudioProfile{
        .mixer = *mixer,
        .eq = *eq,
        .masterLeft = *masterLeft,
        .masterRight = *masterRight,
        .enhancements = *enhancements,
    };
}

std::string serializeAudioSavedJson()
{
    return R"({"status":"saved"})";
}

std::string serializeAudioErrorJson(std::string_view reason)
{
    std::ostringstream out;
    out << R"({"status":"error","reason":")" << reason << R"("})";
    return out.str();
}

std::expected<EnhanceLevel, ParseError> parseEnhanceLevelJson(
    std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    unsigned long level = 0U;
    if (!extractJsonUint(json, "level", level)) {
        return std::unexpected(ParseError::MissingField);
    }
    return EnhanceLevel::tryFromLevel(level);
}

std::expected<bool, ParseError> parseBeepEnabledJson(std::string_view json)
{
    if (json.find('{') == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidJson);
    }

    bool enabled = false;
    if (!extractJsonBool(json, "enabled", enabled)) {
        return std::unexpected(ParseError::MissingField);
    }
    return enabled;
}

} // namespace core
