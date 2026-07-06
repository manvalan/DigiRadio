/**
 * @file    StationService.hpp
 * @brief   Preset list orchestration with NVS persistence and tuner recall.
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

#include "core/ISecureStore.hpp"
#include "core/Station.hpp"
#include "core/StationList.hpp"
#include "core/StationListError.hpp"
#include "core/StoreError.hpp"
#include "core/TunerError.hpp"
#include "tuner/TunerService.hpp"

#include <cstddef>
#include <expected>

namespace station {

/**
 * @brief    StationService — CRUD presets and tune via TunerService.
 *
 * @dname    StationService
 * @return   n/a (type)
 * @pubstate Owns in-memory StationList mirrored to ISecureStore on mutation.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class StationService {
public:
    /**
     * @brief    StationService — bind store and tuner for the process lifetime.
     *
     * @dname    StationService
     * @param    store  Secure persistence for preset JSON.
     * @param    tuner  Tuner orchestration for recall.
     * @pubstate stores references; list empty until loadFromStore().
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    StationService(core::ISecureStore& store, tuner::TunerService& tuner);

    /**
     * @brief    loadFromStore — hydrate list from NVS (empty when absent).
     *
     * @dname    loadFromStore
     * @return   Ok on success, or StoreError / parse failure as IoFailed.
     * @pubstate replaces list_ from NVS JSON when present.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StoreError> loadFromStore();

    /**
     * @brief    list — read the in-memory preset collection.
     *
     * @dname    list
     * @return   Const reference to list_.
     * @pubstate reads list_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const core::StationList& list() const noexcept;

    /**
     * @brief    add — append a preset and persist.
     *
     * @dname    add
     * @param    station  Validated preset from JSON boundary.
     * @return   Ok on success, or StationListError / StoreError.
     * @pubstate mutates list_ and NVS on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StationListError> add(
        core::Station station);

    /**
     * @brief    removeAt — delete preset by index and persist.
     *
     * @dname    removeAt
     * @param    index  Zero-based list position.
     * @return   Ok on success, or StationListError / StoreError.
     * @pubstate mutates list_ and NVS on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StationListError> removeAt(
        std::size_t index);

    /**
     * @brief    reorder — move a preset within the list and persist.
     *
     * @dname    reorder
     * @param    fromIndex  Current list position.
     * @param    toIndex    Target list position.
     * @return   Ok on success, or StationListError.
     * @pubstate mutates list_ and NVS on success.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::StationListError> reorder(
        std::size_t fromIndex, std::size_t toIndex);

    /**
     * @brief    tuneToIndex — recall a saved preset on the tuner.
     *
     * @dname    tuneToIndex
     * @param    index  Zero-based list position.
     * @return   Ok on success, or StationListError / TunerError.
     * @pubstate delegates tune/play to tuner_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::expected<void, core::TunerError> tuneToIndex(
        std::size_t index);

private:
    [[nodiscard]] std::expected<void, core::StoreError> persist();

    core::ISecureStore& store_;
    tuner::TunerService& tuner_;
    core::StationList list_;
};

} // namespace station
