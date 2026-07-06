/**
 * @file    Station.cpp
 * @brief   Station implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/Station.hpp"

namespace core {

Station::Station(StationName name,
                 TunerBand band,
                 std::uint8_t dabFreqIndex,
                 std::optional<std::uint32_t> dabServiceId,
                 std::optional<std::uint32_t> dabComponentId,
                 std::optional<FrequencyKHz> fmFrequency,
                 std::optional<PresetSlot> presetSlot)
    : name_(std::move(name))
    , band_(band)
    , dabFreqIndex_(dabFreqIndex)
    , dabServiceId_(dabServiceId)
    , dabComponentId_(dabComponentId)
    , fmFrequency_(std::move(fmFrequency))
    , presetSlot_(std::move(presetSlot))
{
}

const StationName& Station::name() const noexcept
{
    return name_;
}

TunerBand Station::band() const noexcept
{
    return band_;
}

std::uint8_t Station::dabFreqIndex() const noexcept
{
    return dabFreqIndex_;
}

std::optional<std::uint32_t> Station::dabServiceId() const noexcept
{
    return dabServiceId_;
}

std::optional<std::uint32_t> Station::dabComponentId() const noexcept
{
    return dabComponentId_;
}

std::optional<FrequencyKHz> Station::fmFrequency() const noexcept
{
    return fmFrequency_;
}

std::optional<PresetSlot> Station::presetSlot() const noexcept
{
    return presetSlot_;
}

bool Station::sameTuneTarget(const Station& other) const noexcept
{
    if (band_ != other.band_) {
        return false;
    }
    if (band_ == TunerBand::Fm) {
        return fmFrequency_ && other.fmFrequency_
               && fmFrequency_->value() == other.fmFrequency_->value();
    }
    if (dabFreqIndex_ != other.dabFreqIndex_) {
        return false;
    }
    return dabServiceId_ == other.dabServiceId_
           && dabComponentId_ == other.dabComponentId_;
}

} // namespace core
