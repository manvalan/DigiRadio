/**
 * @file    HealthStatus.hpp
 * @brief   Health-check DTO returned by GET /api/health.
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
#pragma once

#include "core/FirmwareVersion.hpp"
#include "core/CompanionChipStatus.hpp"

#include <optional>

namespace core {

/**
 * @brief    HealthState — coarse health indicator for the API.
 *
 * @dname    HealthState
 * @return   n/a (type)
 * @pubstate n/a
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
enum class HealthState {
    Ok,
};

/**
 * @brief    HealthStatus — immutable health-check response payload.
 *
 * @dname    HealthStatus
 * @param    firmware  Release identifier included in the response.
 * @return   n/a (type)
 * @pubstate Owns state_ and firmware_. Factory ok() builds the nominal
 *           response. No public data members.
 *
 * Pure domain type; serialisation lives in HealthStatusJson.hpp.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
class HealthStatus {
public:
    /**
     * @brief    ok — build the nominal health response.
     *
     * @dname    ok
     * @param    firmware  Active firmware version to report.
     * @return   HealthStatus with HealthState::Ok.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static HealthStatus ok(FirmwareVersion firmware);

    /**
     * @brief    ok — build health response including companion-chip flags.
     *
     * @dname    ok
     * @param    firmware  Active firmware version to report.
     * @param    chips     Si4684 / ADAU1701 / BT1035 boot snapshot.
     * @return   HealthStatus with HealthState::Ok and chips populated.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] static HealthStatus ok(FirmwareVersion firmware,
                                       CompanionChipStatus chips);

    /**
     * @brief    state — read the health indicator.
     *
     * @dname    state
     * @return   Current HealthState.
     * @pubstate reads state_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] HealthState state() const noexcept;

    /**
     * @brief    firmware — read the reported firmware version.
     *
     * @dname    firmware
     * @return   The firmware version carried in this DTO.
     * @pubstate reads firmware_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const FirmwareVersion& firmware() const noexcept;

    /**
     * @brief    chips — optional companion-chip boot flags.
     *
     * @dname    chips
     * @return   Chip status when set by ok(..., chips); otherwise nullopt.
     * @pubstate reads chips_.
     *
     * @author   Michele Bigi
     * @date     2026-07-06
     */
    [[nodiscard]] const std::optional<CompanionChipStatus>& chips() const
        noexcept;

private:
    explicit HealthStatus(HealthState state, FirmwareVersion firmware);

    explicit HealthStatus(HealthState state, FirmwareVersion firmware,
                          CompanionChipStatus chips);

    HealthState state_;
    FirmwareVersion firmware_;
    std::optional<CompanionChipStatus> chips_;
};

} // namespace core
