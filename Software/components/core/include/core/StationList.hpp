/**
 * @file    StationList.hpp
 * @brief   In-memory collection of tuner presets with CRUD validation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include "core/Station.hpp"
#include "core/StationListError.hpp"

#include <cstddef>
#include <expected>
#include <vector>

namespace core {

/**
 * @brief    StationList — ordered preset collection (pure domain).
 *
 * @dname    StationList
 * @return   n/a (type)
 * @pubstate Owns stations_ (max kMaxStations entries). Duplicate tune targets
 *           and preset slots are rejected on add().
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class StationList {
public:
    /** Maximum presets persisted in NVS. */
    static constexpr std::size_t kMaxStations = 20U;

    /**
     * @brief    StationList — construct an empty list.
     *
     * @dname    StationList
     * @pubstate clears stations_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    StationList();

    /**
     * @brief    stations — read the ordered preset vector.
     *
     * @dname    stations
     * @return   Const reference to internal storage.
     * @pubstate reads stations_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const std::vector<Station>& stations() const noexcept;

    /**
     * @brief    replaceAll — replace the entire list (e.g. after NVS load).
     *
     * @dname    replaceAll
     * @param    stations  Validated presets (caller must enforce limits).
     * @pubstate moves stations into stations_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void replaceAll(std::vector<Station> stations);

    /**
     * @brief    add — append a preset after duplicate and capacity checks.
     *
     * @dname    add
     * @param    station  New preset value.
     * @return   Ok on success, or StationListError.
     * @pubstate appends to stations_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, StationListError> add(Station station);

    /**
     * @brief    removeAt — delete preset by list index.
     *
     * @dname    removeAt
     * @param    index  Zero-based position in stations().
     * @return   Ok on success, or StationListError::NotFound.
     * @pubstate erases one element on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, StationListError> removeAt(std::size_t index);

    /**
     * @brief    move — reorder a preset within the list.
     *
     * @dname    move
     * @param    fromIndex  Current index.
     * @param    toIndex    Target index.
     * @return   Ok on success, or StationListError::NotFound.
     * @pubstate reorders stations_ on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, StationListError> move(std::size_t fromIndex,
                                                             std::size_t toIndex);

private:
    [[nodiscard]] bool hasDuplicateTuneTarget(const Station& candidate) const;
    [[nodiscard]] bool slotTaken(const PresetSlot& slot) const;

    std::vector<Station> stations_;
};

} // namespace core
