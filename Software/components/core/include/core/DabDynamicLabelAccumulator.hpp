/**
 * @file    DabDynamicLabelAccumulator.hpp
 * @brief   Reassemble DAB DLS payloads into one dynamic label string.
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

#include "core/BroadcastLabel.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace core {

/**
 * @brief    DabDynamicLabelAccumulator — segmented DAB DLS label buffer.
 *
 * @dname    DabDynamicLabelAccumulator
 * @return   n/a (type)
 * @pubstate Holds partial DLS segments until the label can be read out.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class DabDynamicLabelAccumulator {
public:
    /**
     * @brief    reset — discard accumulated DLS segments.
     *
     * @dname    reset
     * @pubstate clears buffer and segment flags.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void reset() noexcept;

    /**
     * @brief    applySegment — ingest one DLS payload block from the driver.
     *
     * @dname    applySegment
     * @param    segmentIndex  Zero-based segment index from the chip.
     * @param    segmentCount  Total segments advertised for this label.
     * @param    payload       Raw UTF-8 bytes for this segment.
     * @pubstate copies payload into buffer_ at the computed offset.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void applySegment(std::uint16_t segmentIndex,
                      std::uint16_t segmentCount,
                      std::span<const std::uint8_t> payload) noexcept;

    /**
     * @brief    label — read the assembled dynamic label when complete.
     *
     * @dname    label
     * @return   Trimmed DLS text when all segments were received.
     * @pubstate reads buffer_ and segmentValid_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] std::optional<BroadcastLabel> label() const;

private:
    static constexpr std::size_t kMaxSegments = 32U;

    std::array<char, BroadcastLabel::kMaxLength> buffer_{};
    std::array<bool, kMaxSegments> segmentValid_{};
    std::uint16_t expectedSegments_{0U};
};

} // namespace core
