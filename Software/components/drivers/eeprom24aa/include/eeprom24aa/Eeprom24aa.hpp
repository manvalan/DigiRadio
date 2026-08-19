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
#include "core/IdentityError.hpp"

#include "driver/i2c_master.h"

#include <cstdint>
#include <expected>
#include <optional>

namespace eeprom24aa {

/**
 * @brief    Eeprom24aa — reads the factory EUI-48 from Microchip 24AA025E48;
 *           also stores one board-specific calibration byte in the chip's
 *           user-writable region.
 *
 * @dname    Eeprom24aa
 * @return   n/a (type)
 * @pubstate Borrows an existing I2C master bus (shared with ADAU1701). The
 *           EUI-48 lives at word address 0xFA..0xFF (read-only, factory
 *           programmed) per the 24AA025E48 datasheet (DS20001191); the FM
 *           ANTCAP calibration byte lives at word address 0x00 in the
 *           remaining user-writable 250 bytes.
 *
 * @author   Michele Bigi
 * @date     2026-07-07
 */
class Eeprom24aa : public core::IDeviceIdentitySource {
public:
    /** Valid FM ANTCAP calibration values are 0-128 (AN649/AN851); any
     *  stored byte above this, including the EEPROM's blank/erased 0xFF,
     *  reads back as "never calibrated" — no separate sentinel write
     *  needed for a fresh chip. */
    static constexpr std::uint8_t kFmAntCapMax = 128U;

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

    /**
     * @brief    readFmAntCap — read the stored FM antenna calibration byte.
     *
     * @dname    readFmAntCap
     * @return   Calibrated ANTCAP (0-kFmAntCapMax) if one was ever saved via
     *           writeFmAntCap(), nullopt if the byte is blank/out of range,
     *           or IdentityError on an I2C failure.
     * @pubstate performs one I2C read of one byte at word address 0x00.
     *
     * @author   Michele Bigi
     * @date     2026-08-19
     */
    [[nodiscard]] std::expected<std::optional<std::uint8_t>, core::IdentityError>
    readFmAntCap();

    /**
     * @brief    writeFmAntCap — persist an FM antenna calibration value.
     *
     * @dname    writeFmAntCap
     * @param    value  ANTCAP to store, 0-kFmAntCapMax (AN851 Appendix A
     *                  calibration procedure; found via a sweep, not
     *                  computed).
     * @return   Ok on success, or IdentityError::I2cFailed.
     * @pubstate performs one I2C byte write at word address 0x00, then
     *           blocks for the chip's write-cycle time before returning.
     *
     * @author   Michele Bigi
     * @date     2026-08-19
     */
    [[nodiscard]] std::expected<void, core::IdentityError>
    writeFmAntCap(std::uint8_t value);

private:
    i2c_master_bus_handle_t bus_;
    std::uint8_t addr7_;
};

} // namespace eeprom24aa
