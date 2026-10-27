/**
 * @file    AudioPreset.hpp
 * @brief   Named sound preset (EQ + enhancements) and its identifiers.
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

#include "core/AudioEnhancements.hpp"
#include "core/AudioProfile.hpp"
#include "core/EqProfile.hpp"
#include "core/ParseError.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace core {

/**
 * @brief    PresetId — identifies a factory (0..5) or user (10..19) preset.
 *
 * @dname    PresetId
 * @return   n/a (type)
 * @pubstate Owns value_ (immutable after construction).
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class PresetId {
public:
    /** Number of built-in presets. */
    static constexpr std::uint8_t kFactoryCount = 6U;
    /** Number of user preset slots. */
    static constexpr std::uint8_t kUserSlots = 10U;
    /** First id of the user range. */
    static constexpr std::uint8_t kUserBase = 10U;

    /**
     * @brief    tryFromValue — validate a raw id from JSON or storage.
     *
     * @dname    tryFromValue
     * @param    raw  0..5 (factory) or 10..19 (user).
     * @return   PresetId, or ParseError::MissingField when out of range.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] static std::expected<PresetId, ParseError> tryFromValue(
        std::uint32_t raw) noexcept;

    /**
     * @brief    user — id of a user slot (slot must be < kUserSlots).
     *
     * @dname    user
     * @param    slot  0..kUserSlots-1.
     * @return   PresetId in the user range.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] static PresetId user(std::uint8_t slot) noexcept;

    /**
     * @brief    value — raw id as exposed over the API.
     *
     * @dname    value
     * @return   0..5 or 10..19.
     * @pubstate reads value_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::uint8_t value() const noexcept;

    /**
     * @brief    isFactory — true for a built-in preset.
     *
     * @dname    isFactory
     * @return   true when value() < kFactoryCount.
     * @pubstate reads value_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] bool isFactory() const noexcept;

    /**
     * @brief    index — position inside its range (factory or user).
     *
     * @dname    index
     * @return   0-based index.
     * @pubstate reads value_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::uint8_t index() const noexcept;

    friend bool operator==(PresetId, PresetId) noexcept = default;

private:
    explicit PresetId(std::uint8_t value) noexcept;

    std::uint8_t value_;
};

/**
 * @brief    PresetName — validated preset label (1–15 printable bytes).
 *
 * @dname    PresetName
 * @return   n/a (type)
 * @pubstate Owns name_ (immutable after construction).
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
class PresetName {
public:
    /** Maximum label length (bytes). */
    static constexpr std::size_t kMaxLength = 15U;

    /**
     * @brief    tryFrom — validate a label at the HTTP/storage boundary.
     *
     * @dname    tryFrom
     * @param    raw  Untrusted label; no control chars, quotes or backslash.
     * @return   PresetName, or ParseError::MissingField.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] static std::expected<PresetName, ParseError> tryFrom(
        std::string_view raw);

    /**
     * @brief    value — read the label.
     *
     * @dname    value
     * @return   Validated label.
     * @pubstate reads name_.
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] std::string_view value() const noexcept;

private:
    explicit PresetName(std::string name);

    std::string name_;
};

/**
 * @brief    AudioPreset — the part of an AudioProfile a preset recalls.
 *
 * Volume and source are deliberately not part of a preset: recalling a
 * sound preset must never jump the volume or switch the input.
 *
 * @dname    AudioPreset
 * @return   n/a (type)
 * @pubstate Plain value type.
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
struct AudioPreset {
    PresetName name;                ///< Display label.
    EqProfile eq;                   ///< Six-band parametric EQ.
    AudioEnhancements enhancements; ///< Stereo spread and bass boost.

    /**
     * @brief    fromProfile — capture the preset part of a live profile.
     *
     * @dname    fromProfile
     * @param    name     Label for the new preset.
     * @param    profile  Current audio profile.
     * @return   AudioPreset with the profile's EQ and enhancements.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] static AudioPreset fromProfile(PresetName name,
                                                 const AudioProfile& profile);

    /**
     * @brief    appliedTo — overlay this preset on a live profile.
     *
     * @dname    appliedTo
     * @param    profile  Current profile (volume and source are kept).
     * @return   Profile with this preset's EQ and enhancements.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-10-27
     */
    [[nodiscard]] AudioProfile appliedTo(const AudioProfile& profile) const;
};

/**
 * @brief    factoryPresets — the six built-in presets, indexed by id.
 *
 * Neutro, Voce, Musica, Bassi, Classica, Notte, on the factory EQ centre
 * frequencies (20, 100, 400, 1000, 3000, 8000 Hz).
 *
 * @dname    factoryPresets
 * @return   Span of kFactoryCount presets (static storage).
 * @pubstate none
 *
 * @author   Michele Bigi
 * @date     2026-10-27
 */
[[nodiscard]] std::span<const AudioPreset> factoryPresets();

} // namespace core
