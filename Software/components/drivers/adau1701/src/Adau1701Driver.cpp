/**
 * @file    Adau1701Driver.cpp
 * @brief   Adau1701Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "adau1701/Adau1701Driver.hpp"

#include "SigmaStudioFW.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {

/** @brief SigmaStudio default program download (generated C, not part of the C++ API). */
void adau1701_run_default_download(void);

} // extern "C"

namespace adau1701 {

namespace {
constexpr char kTag[] = "Adau1701";
constexpr int kI2cPort = 0;
} // namespace

Adau1701Driver::Adau1701Driver(Adau1701Pins pins)
    : pins_(pins)
    , booted_(false)
    , i2cBus_(nullptr)
    , i2cDev_(nullptr)
{
}

Adau1701Driver::~Adau1701Driver()
{
    auto* dev = static_cast<i2c_master_dev_handle_t>(i2cDev_);
    auto* bus = static_cast<i2c_master_bus_handle_t>(i2cBus_);
    if (dev != nullptr) {
        i2c_master_bus_rm_device(dev);
    }
    if (bus != nullptr) {
        i2c_del_master_bus(bus);
    }
}

std::expected<void, Adau1701Error> Adau1701Driver::boot()
{
    if (booted_) {
        return {};
    }

    gpio_config_t resetCfg = {};
    resetCfg.pin_bit_mask = 1ULL << pins_.resetGpio;
    resetCfg.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&resetCfg) != ESP_OK) {
        return std::unexpected(Adau1701Error::ResetFailed);
    }

    gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    i2c_master_bus_config_t busCfg = {};
    busCfg.i2c_port = static_cast<i2c_port_num_t>(kI2cPort);
    busCfg.sda_io_num = static_cast<gpio_num_t>(pins_.i2cSda);
    busCfg.scl_io_num = static_cast<gpio_num_t>(pins_.i2cScl);
    busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt = 7;
    busCfg.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    if (i2c_new_master_bus(&busCfg, &bus) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_new_master_bus failed");
        return std::unexpected(Adau1701Error::I2cInitFailed);
    }
    i2cBus_ = bus;

    i2c_device_config_t devCfg = {};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = static_cast<uint16_t>(pins_.i2cAddr7);
    devCfg.scl_speed_hz = 100000;

    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(bus, &devCfg, &dev) != ESP_OK) {
        ESP_LOGE(kTag, "i2c_master_bus_add_device failed");
        return std::unexpected(Adau1701Error::I2cInitFailed);
    }
    i2cDev_ = dev;

    sigma_studio_bind_i2c(kI2cPort, static_cast<unsigned char>(pins_.i2cAddr7));
    sigma_studio_set_device(dev);

    adau1701_run_default_download();

    booted_ = true;
    ESP_LOGI(kTag, "SigmaStudio program loaded");
    return {};
}

bool Adau1701Driver::isBooted() const noexcept
{
    return booted_;
}

} // namespace adau1701
