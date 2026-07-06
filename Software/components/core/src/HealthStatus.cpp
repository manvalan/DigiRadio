/**
 * @file    HealthStatus.cpp
 * @brief   HealthStatus implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "core/HealthStatus.hpp"

namespace core {

HealthStatus HealthStatus::ok(FirmwareVersion firmware)
{
    return HealthStatus(HealthState::Ok, std::move(firmware));
}

HealthStatus HealthStatus::ok(FirmwareVersion firmware,
                              CompanionChipStatus chips)
{
    return HealthStatus(HealthState::Ok, std::move(firmware), chips);
}

HealthStatus::HealthStatus(HealthState state, FirmwareVersion firmware)
    : state_(state)
    , firmware_(std::move(firmware))
    , chips_(std::nullopt)
{
}

HealthStatus::HealthStatus(HealthState state, FirmwareVersion firmware,
                           CompanionChipStatus chips)
    : state_(state)
    , firmware_(std::move(firmware))
    , chips_(chips)
{
}

HealthState HealthStatus::state() const noexcept
{
    return state_;
}

const FirmwareVersion& HealthStatus::firmware() const noexcept
{
    return firmware_;
}

const std::optional<CompanionChipStatus>& HealthStatus::chips() const noexcept
{
    return chips_;
}

} // namespace core
