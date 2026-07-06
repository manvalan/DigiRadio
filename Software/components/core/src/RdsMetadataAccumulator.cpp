/**
 * @file    RdsMetadataAccumulator.cpp
 * @brief   RdsMetadataAccumulator implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/RdsMetadataAccumulator.hpp"

namespace core {

void RdsMetadataAccumulator::reset() noexcept
{
    psBuffer_.fill('\0');
    psSegmentValid_.fill(false);
    rtBuffer_.fill('\0');
    rtSegmentValid_.fill(false);
    rtAbFlag_ = false;
    rtAbInitialized_ = false;
}

void RdsMetadataAccumulator::applyGroup(std::uint16_t blockA,
                                        std::uint16_t blockB,
                                        std::uint16_t blockC,
                                        std::uint16_t blockD) noexcept
{
    (void)blockA;
    const std::uint8_t groupType =
        static_cast<std::uint8_t>((blockB >> 12U) & 0x0FU);

    if (groupType == 0U) {
        const std::size_t index =
            static_cast<std::size_t>((blockB >> 1U) & 0x03U);
        if (index < kPsSegments) {
            psBuffer_[index * 2U] =
                static_cast<char>((blockC >> 8U) & 0xFFU);
            psBuffer_[index * 2U + 1U] = static_cast<char>(blockC & 0xFFU);
            psSegmentValid_[index] = true;
        }
        return;
    }

    if (groupType == 2U) {
        const bool abFlag = (blockB & 0x10U) != 0U;
        if (!rtAbInitialized_ || abFlag != rtAbFlag_) {
            rtBuffer_.fill('\0');
            rtSegmentValid_.fill(false);
            rtAbFlag_ = abFlag;
            rtAbInitialized_ = true;
        }
        const std::size_t index =
            static_cast<std::size_t>(blockB & 0x0FU);
        if (index < kRtSegments) {
            rtBuffer_[index * 4U] =
                static_cast<char>((blockC >> 8U) & 0xFFU);
            rtBuffer_[index * 4U + 1U] =
                static_cast<char>(blockC & 0xFFU);
            rtBuffer_[index * 4U + 2U] =
                static_cast<char>((blockD >> 8U) & 0xFFU);
            rtBuffer_[index * 4U + 3U] =
                static_cast<char>(blockD & 0xFFU);
            rtSegmentValid_[index] = true;
        }
    }
}

std::optional<BroadcastLabel> RdsMetadataAccumulator::programName() const
{
    for (bool valid : psSegmentValid_) {
        if (!valid) {
            return std::nullopt;
        }
    }
    return BroadcastLabel::tryFromChipBytes(
        std::string_view(psBuffer_.data(), psBuffer_.size()));
}

std::optional<BroadcastLabel> RdsMetadataAccumulator::radiotext() const
{
    bool any = false;
    for (bool valid : rtSegmentValid_) {
        if (valid) {
            any = true;
            break;
        }
    }
    if (!any) {
        return std::nullopt;
    }
    return BroadcastLabel::tryFromChipBytes(
        std::string_view(rtBuffer_.data(), rtBuffer_.size()));
}

} // namespace core
