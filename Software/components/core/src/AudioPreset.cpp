/**
 * @file    AudioPreset.cpp
 * @brief   PresetId, PresetName, AudioPreset and the factory preset table.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AudioPreset.hpp"

#include <utility>

namespace core {

namespace {

/** Built-in preset as plain data: label, six band gains, stereo, bass. */
struct FactoryRow {
    std::string_view name;
    std::array<float, EqBandIndex::kBandCount> gainsDb;
    std::uint8_t stereo;
    std::uint8_t bass;
};

// Bands: 20 Hz (high-pass shelf, kept flat), 100, 400, 1000, 3000, 8000 Hz.
constexpr std::array<FactoryRow, PresetId::kFactoryCount> kFactoryRows{{
    {"Neutro", {0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F}, 0U, 0U},
    {"Voce", {0.0F, -4.0F, 1.0F, 3.0F, 2.0F, -1.0F}, 0U, 0U},
    {"Musica", {0.0F, 2.0F, 0.0F, 0.0F, 1.0F, 2.0F}, 30U, 20U},
    {"Bassi", {0.0F, 5.0F, 1.0F, 0.0F, 0.0F, 0.0F}, 20U, 60U},
    {"Classica", {0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F}, 40U, 0U},
    {"Notte", {0.0F, -3.0F, 0.0F, 2.0F, 1.0F, -2.0F}, 0U, 0U},
}};

[[nodiscard]] AudioPreset buildFactory(const FactoryRow& row)
{
    EqProfile eq = EqProfile::factoryDefault();
    for (std::uint8_t i = 0; i < EqBandIndex::kBandCount; ++i) {
        const auto index = *EqBandIndex::tryFromIndex(i);
        EqBandSettings band = eq.band(index);
        band.gain = *GainDb::tryFromDb(row.gainsDb[i]);
        eq.setBand(index, band);
    }
    return AudioPreset{
        .name = *PresetName::tryFrom(row.name),
        .eq = eq,
        .enhancements = AudioEnhancements{
            .stereo = *EnhanceLevel::tryFromLevel(row.stereo),
            .bass = *EnhanceLevel::tryFromLevel(row.bass),
        },
    };
}

[[nodiscard]] bool isLabelChar(char c) noexcept
{
    const auto byte = static_cast<unsigned char>(c);
    return byte >= 0x20U && byte != 0x7FU && c != '"' && c != '\\';
}

} // namespace

std::expected<PresetId, ParseError> PresetId::tryFromValue(
    std::uint32_t raw) noexcept
{
    const bool factory = raw < kFactoryCount;
    const bool user = raw >= kUserBase && raw < kUserBase + kUserSlots;
    if (!factory && !user) {
        return std::unexpected(ParseError::MissingField);
    }
    return PresetId(static_cast<std::uint8_t>(raw));
}

PresetId PresetId::user(std::uint8_t slot) noexcept
{
    return PresetId(static_cast<std::uint8_t>(kUserBase + slot));
}

PresetId::PresetId(std::uint8_t value) noexcept
    : value_(value)
{
}

std::uint8_t PresetId::value() const noexcept
{
    return value_;
}

bool PresetId::isFactory() const noexcept
{
    return value_ < kFactoryCount;
}

std::uint8_t PresetId::index() const noexcept
{
    return isFactory() ? value_ : static_cast<std::uint8_t>(value_ - kUserBase);
}

std::expected<PresetName, ParseError> PresetName::tryFrom(std::string_view raw)
{
    if (raw.empty() || raw.size() > kMaxLength) {
        return std::unexpected(ParseError::MissingField);
    }
    for (const char c : raw) {
        if (!isLabelChar(c)) {
            return std::unexpected(ParseError::MissingField);
        }
    }
    return PresetName(std::string(raw));
}

PresetName::PresetName(std::string name)
    : name_(std::move(name))
{
}

std::string_view PresetName::value() const noexcept
{
    return name_;
}

AudioPreset AudioPreset::fromProfile(PresetName name,
                                     const AudioProfile& profile)
{
    return AudioPreset{
        .name = std::move(name),
        .eq = profile.eq,
        .enhancements = profile.enhancements,
    };
}

AudioProfile AudioPreset::appliedTo(const AudioProfile& profile) const
{
    AudioProfile out = profile;
    out.eq = eq;
    out.enhancements = enhancements;
    return out;
}

std::span<const AudioPreset> factoryPresets()
{
    static const std::array<AudioPreset, PresetId::kFactoryCount> kPresets{
        buildFactory(kFactoryRows[0]), buildFactory(kFactoryRows[1]),
        buildFactory(kFactoryRows[2]), buildFactory(kFactoryRows[3]),
        buildFactory(kFactoryRows[4]), buildFactory(kFactoryRows[5]),
    };
    return kPresets;
}

} // namespace core
