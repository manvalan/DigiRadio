/**
 * @file    esp32_i2s_sink.cpp
 * @brief   Shared I2S TX channel towards the ADAU1452 (v2 board).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
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
constexpr std::uint32_t kWriteTimeoutMs = 1000U;

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
    // As v1: wide DMA for network jitter, silence instead of a replayed
    // buffer when a producer falls behind.
    chanCfg.dma_desc_num = 12;
    chanCfg.dma_frame_num = 480;
    chanCfg.auto_clear = true;

    if (i2s_new_channel(&chanCfg, &gTxHandle, nullptr) != ESP_OK) {
        ESP_LOGE(kTag, "i2s_new_channel failed");
        gTxHandle = nullptr;
        return false;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
        // 32-bit slots, 64 BCLK per frame, as the ADAU1452 ports are set.
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(board::pins::I2sBclk),
            .ws = static_cast<gpio_num_t>(board::pins::I2sWs),
            .dout = static_cast<gpio_num_t>(board::pins::I2sDout),
            // DSP -> ESP32 (I2sDin) has no consumer yet.
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
             board::pins::I2sBclk, board::pins::I2sWs, board::pins::I2sDout);
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

bool inUse()
{
    return gInUse.load();
}

bool writeSamples(const std::int32_t* samples, std::size_t sampleCount)
{
    if (gTxHandle == nullptr) {
        return false;
    }
    std::size_t written = 0U;
    // The ADAU1452 is master: without its clocks the DMA never drains, and a
    // bounded wait makes that an error instead of a blocked task.
    const esp_err_t err =
        i2s_channel_write(gTxHandle, samples, sampleCount * sizeof(std::int32_t),
                          &written, pdMS_TO_TICKS(kWriteTimeoutMs));
    return err == ESP_OK;
}

} // namespace esp32_i2s_sink
