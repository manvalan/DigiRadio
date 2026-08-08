/**
 * @file    test_firmware.cpp
 * @brief   Minimal firmware sequence for board, ADAU1701, Si4684 and BT1035 tests.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */

#include "adau1701/Adau1701Dsp.hpp"
#include "adau1701/Adau1701Driver.hpp"
#include "adau1701/EmbeddedDspProgramSource.hpp"
#include "adau1701/FallbackDspProgramSource.hpp"
#include "adau1701/FlashDspProgramSource.hpp"
#include "board_pins.hpp"
#include "bt1035/Bt1035Driver.hpp"
#include "core/Bt1035At.hpp"
#include "core/FrequencyKHz.hpp"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "si4684/Si4684Band.hpp"
#include "si4684/Si4684Driver.hpp"
#include "si4684/Si4684EmbeddedImages.hpp"

#include "test_firmware.hpp"

namespace test_firmware {

namespace {
constexpr char kTag[] = "testfw";

void logChipInfo()
{
    esp_chip_info_t info;
    esp_chip_info(&info);
    ESP_LOGI(kTag, "ESP32 chip: %d cores, rev %d, features=0x%x",
             info.cores, info.revision, info.features);
}

void logError(const char* stage, int code)
{
    ESP_LOGE(kTag, "%s failed (err=%d)", stage, code);
}

void logStage(const char* stage)
{
    ESP_LOGI(kTag, "==== %s ====", stage);
}

const char* a2dpStateToken(core::Bt1035A2dpState state) noexcept
{
    using core::Bt1035A2dpState;
    switch (state) {
    case Bt1035A2dpState::Unsupported:
        return "unsupported";
    case Bt1035A2dpState::Standby:
        return "standby";
    case Bt1035A2dpState::Connecting:
        return "connecting";
    case Bt1035A2dpState::Connected:
        return "connected";
    case Bt1035A2dpState::Streaming:
        return "streaming";
    case Bt1035A2dpState::Paused:
        return "paused";
    }
    return "unknown";
}

} // namespace

void runTestFirmware()
{
    logStage("Board check");
    logChipInfo();

    logStage("Boot ADAU1701");
    adau1701::EmbeddedDspProgramSource embeddedDspProgram;
    adau1701::FlashDspProgramSource flashDspProgram;
    adau1701::FallbackDspProgramSource dspProgramSource(
        flashDspProgram, embeddedDspProgram);
    adau1701::Adau1701Driver adau(
        adau1701::Adau1701Pins{
            .i2cSda = board::pins::Adau1701Sda,
            .i2cScl = board::pins::Adau1701Scl,
            .resetGpio = board::pins::Adau1701Reset,
            .i2cAddr7 = board::pins::Adau1701Addr,
        },
        dspProgramSource);

    if (auto result = adau.boot(); !result) {
        logError("ADAU1701 boot", static_cast<int>(result.error()));
        return;
    }
    ESP_LOGI(kTag, "ADAU1701 boot succeeded");

    logStage("Boot Si4684");
    si4684::Si4684EmbeddedImages images;
    si4684::Si4684Driver si4684(
        si4684::Si4684Pins{
            .spiHost = SPI2_HOST,
            .csGpio = board::pins::Si4684Cs,
            .misoGpio = board::pins::Si4684Miso,
            .mosiGpio = board::pins::Si4684Mosi,
            .sclkGpio = board::pins::Si4684Sclk,
            .rstbGpio = board::pins::Si4684Rstb,
            .intbGpio = board::pins::Si4684Intb,
        },
        images.romPatch(),
        images.dabFirmware(),
        images.fmFirmware());

    if (auto result = si4684.boot(si4684::Si4684Band::Fm); !result) {
        logError("Si4684 boot", static_cast<int>(result.error()));
        return;
    }
    ESP_LOGI(kTag, "Si4684 FM boot succeeded");

    logStage("BT1035 boot");
    bt1035::Bt1035Driver bt(
        bt1035::Bt1035Pins{
            .uartTx = board::pins::Bt1035UartTx,
            .uartRx = board::pins::Bt1035UartRx,
            .resetGpio = board::pins::Bt1035Reset,
            .sysCtlGpio = board::pins::Bt1035SysCtl,
        });

    if (auto result = bt.boot(); !result) {
        logError("BT1035 boot", static_cast<int>(result.error()));
        return;
    }
    ESP_LOGI(kTag, "BT1035 boot succeeded");

    if (auto nameResult = bt.queryDeviceName(); nameResult) {
        ESP_LOGI(kTag, "BT1035 name=%s", nameResult->c_str());
    }

    if (auto result = bt.setDeviceName("DigiRadio-Test"); !result) {
        logError("BT1035 setDeviceName", static_cast<int>(result.error()));
    }

    if (auto result = bt.enterPairingMode(); !result) {
        logError("BT1035 pairing mode", static_cast<int>(result.error()));
    } else {
        ESP_LOGI(kTag, "BT1035 pairing mode enabled");
    }

    if (auto states = bt.queryA2dpState(); states) {
        ESP_LOGI(kTag, "BT1035 A2DP state=%s",
                 a2dpStateToken(states.value()));
    }

    if (auto listResult = bt.queryPairedList(); listResult) {
        ESP_LOGI(kTag, "BT1035 paired devices=%zu", listResult->size());
    }

    logStage("Radio test: tune FM");
    auto frequency = core::FrequencyKHz::tryFromKhz(100700U);
    if (!frequency) {
        ESP_LOGE(kTag, "Invalid FM frequency");
        return;
    }

    if (auto result = si4684.tuneFm(*frequency); !result) {
        logError("Si4684 tune FM", static_cast<int>(result.error()));
    } else {
        ESP_LOGI(kTag, "Si4684 tuned FM to %u kHz", frequency->value());
    }

    if (auto result = si4684.setVolume(16); !result) {
        logError("Si4684 setVolume", static_cast<int>(result.error()));
    }

    if (auto rsqResult = si4684.readFmRsq(); rsqResult) {
        ESP_LOGI(kTag,
                 "FM RSQ: freq=%u kHz rssi=%d dBuV snr=%d dB valid=%d stereo=%d",
                 rsqResult->frequency ? rsqResult->frequency->value() : 0U,
                 static_cast<int>(rsqResult->rssiDbuV),
                 static_cast<int>(rsqResult->snrDb),
                 static_cast<int>(rsqResult->valid),
                 static_cast<int>(rsqResult->stereo));
    } else {
        logError("Si4684 read FM RSQ", static_cast<int>(rsqResult.error()));
    }

    ESP_LOGI(kTag, "Test firmware finished. Keep running for manual pairing and audio verification.");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

} // namespace test_firmware
