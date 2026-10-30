/**
 * @file    AudioPresetBank.cpp
 * @brief   AudioPresetBank implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */

#include "core/AudioPresetBank.hpp"

#include <utility>

namespace core {

std::optional<AudioPreset> AudioPresetBank::find(PresetId id) const
{
    if (id.isFactory()) {
        return factoryPresets()[id.index()];
    }
    return userSlot(id.index());
}

std::optional<AudioPreset> AudioPresetBank::userSlot(std::uint8_t slot) const
{
    if (slot >= PresetId::kUserSlots) {
        return std::nullopt;
    }
    return user_[slot];
}

void AudioPresetBank::store(std::uint8_t slot, AudioPreset preset)
{
    if (slot < PresetId::kUserSlots) {
        user_[slot] = std::move(preset);
    }
}

void AudioPresetBank::erase(std::uint8_t slot)
{
    if (slot >= PresetId::kUserSlots) {
        return;
    }
    user_[slot].reset();
    for (auto& entry : defaults_) {
        if (refersTo(entry, slot)) {
            entry.reset();
        }
    }
    if (refersTo(last_, slot)) {
        last_.reset();
    }
}

std::optional<PresetId> AudioPresetBank::sourceDefault(
    ActiveSource source) const noexcept
{
    const auto index = defaultIndex(source);
    return index ? defaults_[*index] : std::nullopt;
}

void AudioPresetBank::setSourceDefault(ActiveSource source,
                                       std::optional<PresetId> id) noexcept
{
    if (const auto index = defaultIndex(source)) {
        defaults_[*index] = id;
    }
}

std::optional<PresetId> AudioPresetBank::last() const noexcept
{
    return last_;
}

void AudioPresetBank::setLast(std::optional<PresetId> id) noexcept
{
    last_ = id;
}

std::optional<std::size_t> AudioPresetBank::defaultIndex(
    ActiveSource source) noexcept
{
    switch (source) {
    case ActiveSource::Radio:
        return 0U;
    case ActiveSource::Bluetooth:
        return 1U;
    case ActiveSource::Beep:
        return std::nullopt;
    }
    return std::nullopt;
}

bool AudioPresetBank::refersTo(std::optional<PresetId> id,
                               std::uint8_t slot) const noexcept
{
    return id && !id->isFactory() && id->index() == slot;
}

} // namespace core
