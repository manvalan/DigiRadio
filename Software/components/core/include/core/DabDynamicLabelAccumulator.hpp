/**
 * @file    DabDynamicLabelAccumulator.hpp
 * @brief   Reassemble segmented DAB DLS payloads (raw DSRV bytes).
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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace core {

/**
 * @brief    DabDynamicLabelAccumulator — segmented DAB DLS payload buffer.
 *
 * @dname    DabDynamicLabelAccumulator
 * @return   n/a (type)
 * @pubstate Holds the raw bytes of one GET_DIGITAL_SERVICE_DATA payload
 *           while its SEG_NUM/NUM_SEGS segments arrive in order.
 *
 * The Si468x already reassembles DLS messages (AN649 §7.14.2), so most
 * payloads arrive as one segment; this class only joins DSRV segments.
 * The bytes still carry the AN649 Table 21 prefix: DlsDecoder parses them.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class DabDynamicLabelAccumulator {
public:
    /** Largest payload kept: prefix + 128-byte message, with margin. */
    static constexpr std::size_t kMaxBytes = 256U;

    /**
     * @brief    reset — discard accumulated DLS segments.
     *
     * @dname    reset
     * @pubstate clears bytes_ and the segment counters.
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
     * @param    segmentCount  Total segments (0 or 1 = unsegmented).
     * @param    payload       Raw bytes for this segment.
     * @pubstate appends to bytes_; segment 0 restarts the payload and an
     *           out-of-order segment discards it.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    void applySegment(std::uint16_t segmentIndex,
                      std::uint16_t segmentCount,
                      std::span<const std::uint8_t> payload);

    /**
     * @brief    payload — the reassembled payload once every segment arrived.
     *
     * @dname    payload
     * @return   The complete raw payload, or nullopt while incomplete.
     * @pubstate reads bytes_, nextSegment_ and expectedSegments_.
     *
     * @author   Michele Bigi
     * @date     2026-10-26
     */
    [[nodiscard]] std::optional<std::span<const std::uint8_t>>
    payload() const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
    std::uint16_t nextSegment_{0U};
    std::uint16_t expectedSegments_{0U};
};

} // namespace core
