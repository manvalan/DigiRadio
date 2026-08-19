/**
 * @file    Eeprom24aa.cpp
 * @brief   Eeprom24aa implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-07
 */

#include "eeprom24aa/Eeprom24aa.hpp"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>

namespace eeprom24aa {

namespace {

constexpr char kTag[] = "Eeprom24aa";
/** EUI-48 word address per Microchip 24AA025E48 datasheet (DS20001191). */
constexpr std::uint8_t kEui48WordAddress = 0xFAU;
/** FM ANTCAP calibration byte, in the chip's user-writable region (anywhere
 *  below the factory-locked 0xFA-0xFF EUI-48 block). */
constexpr std::uint8_t kFmAntCapWordAddress = 0x00U;
constexpr int kI2cTimeoutMs = 100;
/** DS20001191 §"Page Write"/"Byte Write": max write cycle time after STOP
 *  before the chip acknowledges further I2C traffic. */
constexpr int kI2cWriteCycleMs = 5;

} // namespace

Eeprom24aa::Eeprom24aa(i2c_master_bus_handle_t bus,
                       std::uint8_t addr7) noexcept
    : bus_(bus)
    , addr7_(addr7)
{
}

std::expected<core::DeviceIdentity, core::IdentityError>
Eeprom24aa::readDeviceIdentity()
{
    if (bus_ == nullptr) {
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = addr7_;
    devCfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus_, &devCfg, &dev) != ESP_OK) {
        ESP_LOGW(kTag, "i2c_master_bus_add_device failed");
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    const std::uint8_t wordAddress = kEui48WordAddress;
    std::array<std::uint8_t, 6> payload = {};
    const esp_err_t err = i2c_master_transmit_receive(
        dev, &wordAddress, 1U, payload.data(), payload.size(), kI2cTimeoutMs);

    i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "EUI-48 read failed (err=0x%x)", static_cast<unsigned>(err));
        return std::unexpected(core::IdentityError::ReadFailed);
    }

    return core::DeviceIdentity::fromEui48(core::Eui48::fromBytes(payload));
}

std::expected<std::optional<std::uint8_t>, core::IdentityError>
Eeprom24aa::readFmAntCap()
{
    if (bus_ == nullptr) {
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = addr7_;
    devCfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus_, &devCfg, &dev) != ESP_OK) {
        ESP_LOGW(kTag, "i2c_master_bus_add_device failed");
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    const std::uint8_t wordAddress = kFmAntCapWordAddress;
    std::uint8_t value = 0xFFU;
    const esp_err_t err = i2c_master_transmit_receive(
        dev, &wordAddress, 1U, &value, 1U, kI2cTimeoutMs);

    i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "FM ANTCAP read failed (err=0x%x)",
                 static_cast<unsigned>(err));
        return std::unexpected(core::IdentityError::ReadFailed);
    }

    if (value > kFmAntCapMax) {
        return std::optional<std::uint8_t>{};
    }
    return std::optional<std::uint8_t>{value};
}

std::expected<void, core::IdentityError>
Eeprom24aa::writeFmAntCap(std::uint8_t value)
{
    if (bus_ == nullptr) {
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = addr7_;
    devCfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus_, &devCfg, &dev) != ESP_OK) {
        ESP_LOGW(kTag, "i2c_master_bus_add_device failed");
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    const std::array<std::uint8_t, 2> payload = {kFmAntCapWordAddress, value};
    const esp_err_t err =
        i2c_master_transmit(dev, payload.data(), payload.size(), kI2cTimeoutMs);

    i2c_master_bus_rm_device(dev);

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "FM ANTCAP write failed (err=0x%x)",
                 static_cast<unsigned>(err));
        return std::unexpected(core::IdentityError::I2cFailed);
    }

    vTaskDelay(pdMS_TO_TICKS(kI2cWriteCycleMs));
    return {};
}

} // namespace eeprom24aa
