/**
 * @file    esp32_i2s_sink.cpp
 * @brief   Shared ESP32 -> ADAU1701 I2S TX channel (web radio / phone stream).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp32_i2s_sink.hpp"

#include "board_pins.hpp"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <atomic>

namespace esp32_i2s_sink {

namespace {

constexpr char kTag[] = "i2s_sink";
constexpr int kSampleRateHz = 48000;

i2s_chan_handle_t gTxHandle = nullptr;
std::atomic<bool> gInUse{false};

} // namespace

bool open()
{
    if (gTxHandle != nullptr) {
        return true;
    }

    i2s_chan_config_t chanCfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    // See main/web_radio_stream.cpp history: default (6 x 240 frames =
    // ~30 ms) leaves almost no headroom against producer jitter (network
    // stalls, HTTP scheduling); widen it for both producers that share
    // this channel.
    chanCfg.dma_desc_num = 12;
    chanCfg.dma_frame_num = 480;

    if (i2s_new_channel(&chanCfg, &gTxHandle, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "i2s_new_channel failed");
        gTxHandle = nullptr;
        return false;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
        // 32-bit slots match ADAU1701 SerialOutRegister1 (64 BCLKs/frame).
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(board::pins::I2sBclk),
            .ws = static_cast<gpio_num_t>(board::pins::I2sLrclk),
            .dout = static_cast<gpio_num_t>(board::pins::I2sDataOut),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    if (i2s_channel_init_std_mode(gTxHandle, &stdCfg) != ESP_OK
        || i2s_channel_enable(gTxHandle) != ESP_OK) {
        ESP_LOGE(kTag, "I2S TX channel init/enable failed");
        i2s_del_channel(gTxHandle);
        gTxHandle = nullptr;
        return false;
    }
    ESP_LOGI(kTag, "I2S slave TX started (BCLK=%d WS=%d DOUT=%d)",
             board::pins::I2sBclk, board::pins::I2sLrclk,
             board::pins::I2sDataOut);
    return true;
}

bool tryAcquire()
{
    bool expected = false;
    return gInUse.compare_exchange_strong(expected, true);
}

void release()
{
    gInUse.store(false);
}

bool writeSamples(const std::int32_t* samples, std::size_t sampleCount)
{
    if (gTxHandle == nullptr) {
        return false;
    }
    std::size_t written = 0U;
    const esp_err_t err =
        i2s_channel_write(gTxHandle, samples, sampleCount * sizeof(std::int32_t),
                          &written, portMAX_DELAY);
    return err == ESP_OK;
}

} // namespace esp32_i2s_sink
