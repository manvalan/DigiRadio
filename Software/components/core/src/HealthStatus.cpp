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

namespace {

constexpr std::string_view kSerialUnknown = "unknown";

} // namespace

HealthStatus HealthStatus::ok(FirmwareVersion firmware)
{
    return HealthStatus(HealthState::Ok, std::move(firmware));
}

HealthStatus HealthStatus::ok(FirmwareVersion firmware,
                              CompanionChipStatus chips,
                              std::string_view serialNumber)
{
    return HealthStatus(HealthState::Ok,
                        std::move(firmware),
                        chips,
                        std::string(serialNumber));
}

HealthStatus::HealthStatus(HealthState state, FirmwareVersion firmware)
    : state_(state)
    , firmware_(std::move(firmware))
    , chips_(std::nullopt)
    , serialNumber_(kSerialUnknown)
{
}

HealthStatus::HealthStatus(HealthState state, FirmwareVersion firmware,
                           CompanionChipStatus chips,
                           std::string serialNumber)
    : state_(state)
    , firmware_(std::move(firmware))
    , chips_(chips)
    , serialNumber_(std::move(serialNumber))
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

std::string_view HealthStatus::serialNumber() const noexcept
{
    return serialNumber_;
}

} // namespace core
