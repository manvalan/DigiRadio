/**
 * @file    HealthStatus.cpp
 * @brief   HealthStatus implementation.
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
 * @date    2026-07-06
 */

#include "core/HealthStatus.hpp"

namespace core {

HealthStatus HealthStatus::ok(FirmwareVersion firmware)
{
    return HealthStatus(HealthState::Ok, std::move(firmware));
}

HealthStatus::HealthStatus(HealthState state, FirmwareVersion firmware)
    : state_(state)
    , firmware_(std::move(firmware))
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

} // namespace core
