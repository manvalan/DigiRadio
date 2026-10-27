/**
 * @file    DabDynamicLabelAccumulator.cpp
 * @brief   DabDynamicLabelAccumulator implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/DabDynamicLabelAccumulator.hpp"

namespace core {

void DabDynamicLabelAccumulator::reset() noexcept
{
    bytes_.clear();
    nextSegment_ = 0U;
    expectedSegments_ = 0U;
}

void DabDynamicLabelAccumulator::applySegment(
    std::uint16_t segmentIndex,
    std::uint16_t segmentCount,
    std::span<const std::uint8_t> payload)
{
    const std::uint16_t count = segmentCount == 0U ? 1U : segmentCount;
    if (segmentIndex == 0U) {
        reset();
        expectedSegments_ = count;
    }
    if (count != expectedSegments_ || segmentIndex != nextSegment_
        || bytes_.size() + payload.size() > kMaxBytes) {
        reset();
        return;
    }
    bytes_.insert(bytes_.end(), payload.begin(), payload.end());
    ++nextSegment_;
}

std::optional<std::span<const std::uint8_t>>
DabDynamicLabelAccumulator::payload() const noexcept
{
    if (expectedSegments_ == 0U || nextSegment_ != expectedSegments_) {
        return std::nullopt;
    }
    return std::span<const std::uint8_t>(bytes_);
}

} // namespace core
