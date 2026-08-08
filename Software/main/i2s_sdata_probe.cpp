/**
 * @file    i2s_sdata_probe.cpp
 * @brief   One-shot I2S slave RX capture to check for real audio on SDATA.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "i2s_sdata_probe.hpp"

#include "board_pins.hpp"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include <cstddef>
#include <cstdint>

namespace i2s_sdata_probe {

namespace {

constexpr char kTag[] = "i2s_sdata_probe";
constexpr int kSampleRateHz = 48000;
constexpr std::size_t kReadBufSamples = 512U;
// Top 24 bits carry ADAU audio data in a 32-bit slot (see former
// esp32_i2s_tone.cpp); a few % of full scale is already audible content.
constexpr std::int32_t kAudibleThreshold = 0x7FFFFFFF / 20; // ~5% FS

struct CaptureStats {
    std::int32_t peak = 0;
    std::uint32_t nonZeroSamples = 0U;
    std::uint32_t totalSamples = 0U;
};

i2s_chan_handle_t openRxChannel()
{
    i2s_chan_config_t chanCfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    i2s_chan_handle_t rxHandle = nullptr;
    if (i2s_new_channel(&chanCfg, nullptr, &rxHandle) != ESP_OK) {
        ESP_LOGW(kTag, "i2s_new_channel failed");
        return nullptr;
    }

    i2s_std_config_t stdCfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRateHz),
        // 32-bit slots match ADAU1701 SerialOutRegister1=0x800 (64 BCLKs/frame),
        // same format the removed esp32_i2s_tone.cpp used on the TX side.
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(board::pins::I2sBclk),
            .ws = static_cast<gpio_num_t>(board::pins::I2sLrclk),
            .dout = I2S_GPIO_UNUSED,
            .din = static_cast<gpio_num_t>(board::pins::I2sDataOut),
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    // Slave: BCLK/LRCLK driven by ADAU1701 master; no clock overrides needed.

    if (i2s_channel_init_std_mode(rxHandle, &stdCfg) != ESP_OK
        || i2s_channel_enable(rxHandle) != ESP_OK) {
        ESP_LOGW(kTag, "I2S RX channel init/enable failed");
        i2s_del_channel(rxHandle);
        return nullptr;
    }

    ESP_LOGI(kTag, "I2S slave RX started (BCLK=%d WS=%d DIN=%d)",
             board::pins::I2sBclk, board::pins::I2sLrclk,
             board::pins::I2sDataOut);
    return rxHandle;
}

void accumulateSamples(const std::int32_t* buf, std::size_t count,
                       CaptureStats& stats)
{
    for (std::size_t i = 0U; i < count; ++i) {
        const std::int32_t value = buf[i];
        const std::int32_t absValue = value < 0 ? -value : value;
        if (absValue > stats.peak) {
            stats.peak = absValue;
        }
        if (value != 0) {
            ++stats.nonZeroSamples;
        }
    }
    stats.totalSamples += static_cast<std::uint32_t>(count);
}

CaptureStats sampleWindow(i2s_chan_handle_t rxHandle, int windowMs)
{
    CaptureStats stats;
    std::int32_t buf[kReadBufSamples];
    const std::int64_t startUs = esp_timer_get_time();
    const std::int64_t targetUs = static_cast<std::int64_t>(windowMs) * 1000;
    while (esp_timer_get_time() - startUs < targetUs) {
        std::size_t bytesRead = 0U;
        const esp_err_t err = i2s_channel_read(rxHandle, buf, sizeof(buf),
                                               &bytesRead, pdMS_TO_TICKS(50));
        if (err != ESP_OK || bytesRead == 0U) {
            continue;
        }
        accumulateSamples(buf, bytesRead / sizeof(std::int32_t), stats);
    }
    return stats;
}

const char* verdictFor(const CaptureStats& stats)
{
    if (stats.totalSamples == 0U || stats.nonZeroSamples == 0U) {
        return "SILENT — dead/zero line on GPIO16";
    }
    return stats.peak >= kAudibleThreshold
        ? "real audio-level content"
        : "activity present but very low amplitude";
}

} // namespace

void captureAndLog(int windowMs)
{
    i2s_chan_handle_t rxHandle = openRxChannel();
    if (rxHandle == nullptr) {
        return;
    }

    const CaptureStats stats = sampleWindow(rxHandle, windowMs);

    i2s_channel_disable(rxHandle);
    i2s_del_channel(rxHandle);

    ESP_LOGI(kTag, "SDATA capture: %u samples, peak=%ld, nonzero=%u/%u -> %s",
             static_cast<unsigned>(stats.totalSamples),
             static_cast<long>(stats.peak),
             static_cast<unsigned>(stats.nonZeroSamples),
             static_cast<unsigned>(stats.totalSamples), verdictFor(stats));
}

} // namespace i2s_sdata_probe
