/**
 * @file    AudioPresetBank.hpp
 * @brief   User preset slots, per-source default presets and last recall.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-27
 */
#pragma once

#include "core/ActiveSource.hpp"
#include "core/AudioPreset.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace core {

/**
 * @brief    AudioPresetBank — everything the preset feature persists.
 *
 * Factory presets are not stored: find() resolves them from
 * factoryPresets(). The bank holds the ten user slots, the preset to
 * recall automatically per input (radio, ESP32 stream) and the last
 * preset the user recalled.
 *
 * @dname    AudioPresetBank
 * @return   n/a (type)
 * @pubstate Owns user_, defaults_, last_.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class AudioPresetBank {
public:
    /**
     * @brief    find — resolve a factory or user preset.
     *
     * @dname    find
     * @param    id  Preset to look up.
     * @return   The preset, or nullopt for an empty user slot.
     * @pubstate reads user_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::optional<AudioPreset> find(PresetId id) const;

    /**
     * @brief    userSlot — read one user slot.
     *
     * @dname    userSlot
     * @param    slot  0..PresetId::kUserSlots-1.
     * @return   Stored preset, or nullopt when empty or out of range.
     * @pubstate reads user_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::optional<AudioPreset> userSlot(std::uint8_t slot) const;

    /**
     * @brief    store — write a user slot.
     *
     * @dname    store
     * @param    slot    0..PresetId::kUserSlots-1 (ignored when out of range).
     * @param    preset  Preset to store.
     * @pubstate writes user_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    void store(std::uint8_t slot, AudioPreset preset);

    /**
     * @brief    erase — empty a user slot and drop references to it.
     *
     * @dname    erase
     * @param    slot  0..PresetId::kUserSlots-1.
     * @pubstate writes user_, defaults_, last_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    void erase(std::uint8_t slot);

    /**
     * @brief    sourceDefault — preset recalled when an input is selected.
     *
     * @dname    sourceDefault
     * @param    source  Radio or Bluetooth (ESP32 stream); Beep has none.
     * @return   Preset id, or nullopt for "keep the current sound".
     * @pubstate reads defaults_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::optional<PresetId> sourceDefault(
        ActiveSource source) const noexcept;

    /**
     * @brief    setSourceDefault — set or clear an input's default preset.
     *
     * @dname    setSourceDefault
     * @param    source  Radio or Bluetooth (Beep is ignored).
     * @param    id      Preset id, or nullopt to clear.
     * @pubstate writes defaults_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    void setSourceDefault(ActiveSource source,
                          std::optional<PresetId> id) noexcept;

    /**
     * @brief    last — the preset the user recalled most recently.
     *
     * @dname    last
     * @return   Preset id, or nullopt when none was recalled.
     * @pubstate reads last_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::optional<PresetId> last() const noexcept;

    /**
     * @brief    setLast — remember the preset just recalled.
     *
     * @dname    setLast
     * @param    id  Preset id, or nullopt after a manual EQ edit.
     * @pubstate writes last_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    void setLast(std::optional<PresetId> id) noexcept;

private:
    [[nodiscard]] static std::optional<std::size_t> defaultIndex(
        ActiveSource source) noexcept;
    [[nodiscard]] bool refersTo(std::optional<PresetId> id,
                                std::uint8_t slot) const noexcept;

    std::array<std::optional<AudioPreset>, PresetId::kUserSlots> user_{};
    std::array<std::optional<PresetId>, 2> defaults_{};
    std::optional<PresetId> last_{};
};

} // namespace core
