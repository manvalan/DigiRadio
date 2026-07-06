/**
 * @file    StationService.cpp
 * @brief   StationService implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "station/StationService.hpp"

#include "core/StationListJson.hpp"
#include "core/TunerBand.hpp"

namespace station {

StationService::StationService(core::ISecureStore& store,
                               tuner::TunerService& tuner)
    : store_(store)
    , tuner_(tuner)
{
}

std::expected<void, core::StoreError> StationService::loadFromStore()
{
    if (!store_.hasStationList()) {
        list_ = core::StationList{};
        return {};
    }

    auto blob = store_.loadStationListJson();
    if (!blob) {
        return std::unexpected(blob.error());
    }

    auto parsed = core::parseStationListJson(*blob);
    if (!parsed) {
        return std::unexpected(core::StoreError::InvalidData);
    }
    list_ = std::move(*parsed);
    return {};
}

const core::StationList& StationService::list() const noexcept
{
    return list_;
}

std::expected<void, core::StoreError> StationService::persist()
{
    return store_.saveStationListJson(core::serializeStationListJson(list_));
}

std::expected<void, core::StationListError> StationService::add(
    core::Station station)
{
    if (auto added = list_.add(std::move(station)); !added) {
        return added;
    }
    if (auto saved = persist(); !saved) {
        list_.removeAt(list_.stations().size() - 1U);
        return std::unexpected(core::StationListError::Full);
    }
    return {};
}

std::expected<void, core::StationListError> StationService::removeAt(
    std::size_t index)
{
    if (auto removed = list_.removeAt(index); !removed) {
        return removed;
    }
    if (auto saved = persist(); !saved) {
        return std::unexpected(core::StationListError::NotFound);
    }
    return {};
}

std::expected<void, core::TunerError> StationService::tuneToIndex(
    std::size_t index)
{
    if (index >= list_.stations().size()) {
        return std::unexpected(core::TunerError::NotBooted);
    }

    const core::Station& station = list_.stations()[index];
    if (station.band() == core::TunerBand::Fm) {
        if (!station.fmFrequency()) {
            return std::unexpected(core::TunerError::WrongBand);
        }
        return tuner_.tuneFm(*station.fmFrequency());
    }

    if (auto tuned = tuner_.tuneDab(station.dabFreqIndex()); !tuned) {
        return tuned;
    }

    if (station.dabServiceId() && station.dabComponentId()) {
        return tuner_.playDabService(*station.dabServiceId(),
                                     *station.dabComponentId());
    }
    return {};
}

} // namespace station
