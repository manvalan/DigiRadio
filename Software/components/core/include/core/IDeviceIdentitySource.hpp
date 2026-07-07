/**
 * @file    IDeviceIdentitySource.hpp
 * @brief   Port for reading per-board identity from non-volatile storage.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */
#pragma once

#include "core/DeviceIdentity.hpp"
#include "core/IdentityError.hpp"

#include <expected>

namespace core {

/**
 * @brief    IDeviceIdentitySource — reads DeviceIdentity from board storage.
 *
 * @dname    IDeviceIdentitySource
 * @return   n/a (type)
 * @pubstate Implementations live in the imperative shell (EEPROM driver).
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class IDeviceIdentitySource {
public:
    virtual ~IDeviceIdentitySource() = default;

    /**
     * @brief    readDeviceIdentity — load identity from the backing store.
     *
     * @dname    readDeviceIdentity
     * @return   DeviceIdentity on success, or IdentityError.
     * @pubstate none
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] virtual std::expected<DeviceIdentity, IdentityError>
    readDeviceIdentity() = 0;
};

} // namespace core
