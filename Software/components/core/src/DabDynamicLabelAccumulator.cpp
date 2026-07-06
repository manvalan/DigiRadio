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

#include <algorithm>
#include <cstring>

namespace core {

void DabDynamicLabelAccumulator::reset() noexcept
{
    buffer_.fill('\0');
    segmentValid_.fill(false);
    expectedSegments_ = 0U;
}

void DabDynamicLabelAccumulator::applySegment(
    std::uint16_t segmentIndex,
    std::uint16_t segmentCount,
    std::span<const std::uint8_t> payload) noexcept
{
    if (segmentCount == 0U || segmentCount > kMaxSegments) {
        return;
    }
    if (segmentIndex >= segmentCount) {
        return;
    }
    if (expectedSegments_ != segmentCount) {
        reset();
        expectedSegments_ = segmentCount;
    }

    const std::size_t offset =
        static_cast<std::size_t>(segmentIndex)
        * static_cast<std::size_t>(payload.size());
    if (offset >= buffer_.size()) {
        return;
    }
    const std::size_t copyLen =
        std::min(payload.size(), buffer_.size() - offset);
    std::memcpy(buffer_.data() + offset, payload.data(), copyLen);
    segmentValid_[segmentIndex] = true;
}

std::optional<BroadcastLabel> DabDynamicLabelAccumulator::label() const
{
    if (expectedSegments_ == 0U) {
        return std::nullopt;
    }
    for (std::uint16_t i = 0U; i < expectedSegments_; ++i) {
        if (!segmentValid_[i]) {
            return std::nullopt;
        }
    }
    return BroadcastLabel::tryFromChipBytes(
        std::string_view(buffer_.data(), buffer_.size()));
}

} // namespace core
