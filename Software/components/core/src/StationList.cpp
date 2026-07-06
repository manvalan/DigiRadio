/**
 * @file    StationList.cpp
 * @brief   StationList implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/StationList.hpp"

#include <algorithm>

namespace core {

StationList::StationList() = default;

const std::vector<Station>& StationList::stations() const noexcept
{
    return stations_;
}

void StationList::replaceAll(std::vector<Station> stations)
{
    stations_ = std::move(stations);
}

bool StationList::hasDuplicateTuneTarget(const Station& candidate) const
{
    for (const Station& existing : stations_) {
        if (existing.sameTuneTarget(candidate)) {
            return true;
        }
    }
    return false;
}

bool StationList::slotTaken(const PresetSlot& slot) const
{
    for (const Station& existing : stations_) {
        if (existing.presetSlot() && existing.presetSlot()->value() == slot.value()) {
            return true;
        }
    }
    return false;
}

std::expected<void, StationListError> StationList::add(Station station)
{
    if (stations_.size() >= kMaxStations) {
        return std::unexpected(StationListError::Full);
    }
    if (hasDuplicateTuneTarget(station)) {
        return std::unexpected(StationListError::Duplicate);
    }
    if (station.presetSlot() && slotTaken(*station.presetSlot())) {
        return std::unexpected(StationListError::SlotInUse);
    }
    stations_.push_back(std::move(station));
    return {};
}

std::expected<void, StationListError> StationList::removeAt(std::size_t index)
{
    if (index >= stations_.size()) {
        return std::unexpected(StationListError::NotFound);
    }
    stations_.erase(stations_.begin() + static_cast<std::ptrdiff_t>(index));
    return {};
}

std::expected<void, StationListError> StationList::move(std::size_t fromIndex,
                                                        std::size_t toIndex)
{
    if (fromIndex >= stations_.size() || toIndex >= stations_.size()) {
        return std::unexpected(StationListError::NotFound);
    }
    if (fromIndex == toIndex) {
        return {};
    }
    Station moving = std::move(stations_[fromIndex]);
    stations_.erase(stations_.begin() + static_cast<std::ptrdiff_t>(fromIndex));
    stations_.insert(stations_.begin() + static_cast<std::ptrdiff_t>(toIndex),
                     std::move(moving));
    return {};
}

} // namespace core
