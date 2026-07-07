/**
 * @file    Eeprom24aa.hpp
 * @brief   24AA025E48 EEPROM driver — factory EUI-48 on the shared I2C bus.
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

#include "core/IDeviceIdentitySource.hpp"

#include "driver/i2c_master.h"

#include <cstdint>

namespace eeprom24aa {

/**
 * @brief    Eeprom24aa — reads the factory EUI-48 from Microchip 24AA025E48.
 *
 * @dname    Eeprom24aa
 * @return   n/a (type)
 * @pubstate Borrows an existing I2C master bus (shared with ADAU1701). The
 *           EUI-48 lives at word address 0xFA..0xFF per the 24AA025E48
 *           datasheet (DS20001191).
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class Eeprom24aa : public core::IDeviceIdentitySource {
public:
    /**
     * @brief    Eeprom24aa — bind to a running I2C master bus and 7-bit addr.
     *
     * @dname    Eeprom24aa
     * @param    bus     Shared I2C bus handle from Adau1701Driver after boot.
     * @param    addr7   7-bit EEPROM address (0x52 on DigiRadio).
     * @pubstate stores bus_ and addr7_; does not own the bus.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    Eeprom24aa(i2c_master_bus_handle_t bus, std::uint8_t addr7) noexcept;

    /**
     * @brief    readDeviceIdentity — read EUI-48 and derive DeviceIdentity.
     *
     * @dname    readDeviceIdentity
     * @return   DeviceIdentity on success, or IdentityError.
     * @pubstate performs one I2C read of six bytes at word address 0xFA.
     *
     * @author   Michele Bigi
     * @date     2026-07-07
     */
    [[nodiscard]] std::expected<core::DeviceIdentity, core::IdentityError>
    readDeviceIdentity() override;

private:
    i2c_master_bus_handle_t bus_;
    std::uint8_t addr7_;
};

} // namespace eeprom24aa
