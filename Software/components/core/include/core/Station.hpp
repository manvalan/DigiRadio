/**
 * @file    Station.hpp
 * @brief   Value type for one saved tuner preset (DAB or FM).
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

#include "core/FrequencyKHz.hpp"
#include "core/PresetSlot.hpp"
#include "core/StationName.hpp"
#include "core/TunerBand.hpp"

#include <cstdint>
#include <optional>

namespace core {

/**
 * @brief    Station — immutable preset: label, band, tune target, optional slot.
 *
 * @dname    Station
 * @return   n/a (type)
 * @pubstate All fields fixed at construction. FM presets carry fmFrequency_;
 *           DAB presets carry dabFreqIndex_ and optional service/component ids.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class Station {
public:
    /**
     * @brief    Station — construct a validated preset value.
     *
     * @dname    Station
     * @param    name            User-visible label.
     * @param    band            DAB or FM target band.
     * @param    dabFreqIndex    Ensemble index 0–37 when band is Dab.
     * @param    dabServiceId    Optional DAB service id for play().
     * @param    dabComponentId  Optional DAB component id for play().
     * @param    fmFrequency     FM centre frequency when band is Fm.
     * @param    presetSlot      Optional hardware preset button slot.
     * @pubstate stores immutable tune target fields.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    Station(StationName name,
            TunerBand band,
            std::uint8_t dabFreqIndex,
            std::optional<std::uint32_t> dabServiceId,
            std::optional<std::uint32_t> dabComponentId,
            std::optional<FrequencyKHz> fmFrequency,
            std::optional<PresetSlot> presetSlot);

    /**
     * @brief    name — read the preset display label.
     *
     * @dname    name
     * @return   Station name value.
     * @pubstate reads name_.
     */
    [[nodiscard]] const StationName& name() const noexcept;

    /**
     * @brief    band — read the target RF band.
     *
     * @dname    band
     * @return   Dab or Fm band selector.
     * @pubstate reads band_.
     */
    [[nodiscard]] TunerBand band() const noexcept;

    /**
     * @brief    dabFreqIndex — read the Band III ensemble index.
     *
     * @dname    dabFreqIndex
     * @return   Ensemble index 0–37.
     * @pubstate reads dabFreqIndex_.
     */
    [[nodiscard]] std::uint8_t dabFreqIndex() const noexcept;

    /**
     * @brief    dabServiceId — read optional DAB service id for play().
     *
     * @dname    dabServiceId
     * @return   Service id when stored, otherwise empty.
     * @pubstate reads dabServiceId_.
     */
    [[nodiscard]] std::optional<std::uint32_t> dabServiceId() const noexcept;

    /**
     * @brief    dabComponentId — read optional DAB component id for play().
     *
     * @dname    dabComponentId
     * @return   Component id when stored, otherwise empty.
     * @pubstate reads dabComponentId_.
     */
    [[nodiscard]] std::optional<std::uint32_t> dabComponentId() const noexcept;

    /**
     * @brief    fmFrequency — read FM centre frequency when band is Fm.
     *
     * @dname    fmFrequency
     * @return   Frequency when stored, otherwise empty.
     * @pubstate reads fmFrequency_.
     */
    [[nodiscard]] std::optional<FrequencyKHz> fmFrequency() const noexcept;

    /**
     * @brief    presetSlot — read optional hardware preset button slot.
     *
     * @dname    presetSlot
     * @return   Preset slot when assigned, otherwise empty.
     * @pubstate reads presetSlot_.
     */
    [[nodiscard]] std::optional<PresetSlot> presetSlot() const noexcept;

    /**
     * @brief    sameTuneTarget — compare band-specific tune coordinates.
     *
     * @dname    sameTuneTarget
     * @param    other  Candidate preset.
     * @return   true when both would tune/play the same source.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] bool sameTuneTarget(const Station& other) const noexcept;

private:
    StationName name_;
    TunerBand band_;
    std::uint8_t dabFreqIndex_;
    std::optional<std::uint32_t> dabServiceId_;
    std::optional<std::uint32_t> dabComponentId_;
    std::optional<FrequencyKHz> fmFrequency_;
    std::optional<PresetSlot> presetSlot_;
};

} // namespace core
