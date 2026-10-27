/**
 * @file    DabReturnPolicy.cpp
 * @brief   DabReturnPolicy implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "core/DabReturnPolicy.hpp"

#include <algorithm>

namespace core {

void DabReturnPolicy::setIntervalMinutes(std::uint16_t minutes) noexcept
{
    minutes_ = std::min(minutes, kMaxDabReturnMinutes);
}

void DabReturnPolicy::arm(const DabReturnTarget& target,
                          std::uint32_t nowMs) noexcept
{
    target_ = target;
    lastTryMs_ = nowMs;
}

void DabReturnPolicy::cancel() noexcept
{
    target_.reset();
}

const std::optional<DabReturnTarget>& DabReturnPolicy::target() const noexcept
{
    return target_;
}

bool DabReturnPolicy::isStale(const TunerStatus& status) const noexcept
{
    if (!target_) {
        return false;
    }
    if (status.band != TunerBand::Fm) {
        return true;
    }
    return status.fmProgramId && *status.fmProgramId != target_->pi;
}

bool DabReturnPolicy::shouldTry(const TunerStatus& status,
                                std::uint32_t nowMs) const noexcept
{
    if (!target_ || minutes_ == 0U || isStale(status)) {
        return false;
    }
    const std::uint32_t intervalMs = static_cast<std::uint32_t>(minutes_)
                                     * 60U * 1000U;
    return nowMs - lastTryMs_ >= intervalMs;
}

void DabReturnPolicy::recordAttempt(std::uint32_t nowMs) noexcept
{
    lastTryMs_ = nowMs;
}

bool DabReturnPolicy::dabGoodEnough(const TunerStatus& status) noexcept
{
    return status.band == TunerBand::Dab && status.locked
           && status.dabFicQuality.value_or(0U) >= kDabReturnMinFicQuality;
}

} // namespace core
