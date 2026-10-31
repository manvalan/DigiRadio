/**
 * @file    board_v2.cpp
 * @brief   v2 board power domain and control buses.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "board_v2.hpp"

#include "board_pins.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>

namespace board_v2 {

namespace {

constexpr char kTag[] = "board_v2";
/** TPS63031 soft start plus the TPS7A20 and the DVDD regulator behind it. */
constexpr int kRailSettleMs = 20;
/** Largest Si4684 reply (Si4684Driver kMaxReplyBytes) sets the bus limit. */
constexpr int kSpiMaxTransfer = 8224;

/** Everything on the 3V3_AUDIO side. ADAU_RESET# has its own pull-down. */
constexpr std::array<int, 17> kAudioPins = {
    board::pins::SpiSclk,    board::pins::SpiMosi,   board::pins::SpiMiso,
    board::pins::Si4684Cs,   board::pins::Si4684Rstb, board::pins::Si4684Intb,
    board::pins::Si4684Smode, board::pins::AdauCs,   board::pins::AdauReset,
    board::pins::I2sBclk,    board::pins::I2sWs,     board::pins::I2sDout,
    board::pins::I2sDin,     board::pins::BtUartRx,  board::pins::BtUartTx,
    board::pins::BtSysCtrl,  board::pins::AdauSelfBoot,
};

i2c_master_bus_handle_t gI2c = nullptr;
bool gSpiReady = false;

void releaseAudioPins()
{
    for (const int pin : kAudioPins) {
        gpio_reset_pin(static_cast<gpio_num_t>(pin));
        gpio_set_direction(static_cast<gpio_num_t>(pin), GPIO_MODE_DISABLE);
        gpio_set_pull_mode(static_cast<gpio_num_t>(pin), GPIO_FLOATING);
    }
    gpio_reset_pin(static_cast<gpio_num_t>(board::pins::BtReset));
    gpio_set_direction(static_cast<gpio_num_t>(board::pins::BtReset),
                       GPIO_MODE_DISABLE);
}

[[nodiscard]] bool initSpi()
{
    if (gSpiReady) {
        return true;
    }
    spi_bus_config_t cfg = {};
    cfg.miso_io_num = board::pins::SpiMiso;
    cfg.mosi_io_num = board::pins::SpiMosi;
    cfg.sclk_io_num = board::pins::SpiSclk;
    cfg.quadwp_io_num = -1;
    cfg.quadhd_io_num = -1;
    cfg.max_transfer_sz = kSpiMaxTransfer;
    gSpiReady =
        spi_bus_initialize(SPI2_HOST, &cfg, SPI_DMA_CH_AUTO) == ESP_OK;
    return gSpiReady;
}

[[nodiscard]] bool initI2c()
{
    if (gI2c != nullptr) {
        return true;
    }
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = 0;
    cfg.sda_io_num = static_cast<gpio_num_t>(board::pins::I2cSda);
    cfg.scl_io_num = static_cast<gpio_num_t>(board::pins::I2cScl);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    return i2c_new_master_bus(&cfg, &gI2c) == ESP_OK;
}

} // namespace

bool enableAnalog()
{
    releaseAudioPins();
    // R22 keeps the rail off until this pin takes over; the handler drops it
    // again before any software restart.
    gpio_set_level(static_cast<gpio_num_t>(board::pins::EnableAnalog), 1);
    if (gpio_set_direction(static_cast<gpio_num_t>(board::pins::EnableAnalog),
                           GPIO_MODE_OUTPUT) != ESP_OK) {
        ESP_LOGE(kTag, "ENABLE_ANALOG pin failed");
        return false;
    }
    (void)esp_register_shutdown_handler(&disableAnalog);
    vTaskDelay(pdMS_TO_TICKS(kRailSettleMs));

    // Tied to GND on v1 (AN649: SPI mode); here the ESP32 holds it.
    gpio_set_level(static_cast<gpio_num_t>(board::pins::Si4684Smode), 0);
    gpio_set_direction(static_cast<gpio_num_t>(board::pins::Si4684Smode),
                       GPIO_MODE_OUTPUT);
    return true;
}

void disableAnalog()
{
    releaseAudioPins();
    gpio_set_level(static_cast<gpio_num_t>(board::pins::EnableAnalog), 0);
}

bool initBuses()
{
    if (!initSpi() || !initI2c()) {
        ESP_LOGE(kTag, "control bus init failed");
        return false;
    }
    return true;
}

i2c_master_bus_handle_t i2cBus() noexcept
{
    return gI2c;
}

} // namespace board_v2
