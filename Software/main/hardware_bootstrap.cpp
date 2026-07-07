/**
 * @file    hardware_bootstrap.cpp
 * @brief   HardwareBootstrap implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "hardware_bootstrap.hpp"

#include "adau1701/Adau1701Driver.hpp"
#include "adau1701/Adau1701Dsp.hpp"
#include "adau1701/EmbeddedDspProgramSource.hpp"
#include "adau1701/FallbackDspProgramSource.hpp"
#include "adau1701/FlashDspProgramSource.hpp"
#include "audio/AudioService.hpp"
#include "board_pins.hpp"
#include "bt1035/Bt1035Driver.hpp"
#include "core/DeviceIdentity.hpp"
#include "driver/i2c_master.h"
#include "eeprom24aa/Eeprom24aa.hpp"
#include "secure_store/NvsAudioProfileStore.hpp"
#include "si4684/Si4684Band.hpp"
#include "si4684/Si4684Driver.hpp"
#include "si4684/Si4684EmbeddedImages.hpp"
#include "si4684/Si4684Tuner.hpp"

#include "driver/spi_master.h"
#include "esp_log.h"

namespace hardware {

namespace {
constexpr char kTag[] = "hw_boot";

si4684::Si4684EmbeddedImages gImages;
si4684::Si4684Driver gSi4684(
    si4684::Si4684Pins{
        .spiHost = SPI2_HOST,
        .csGpio = board::pins::Si4684Cs,
        .misoGpio = board::pins::Si4684Miso,
        .mosiGpio = board::pins::Si4684Mosi,
        .sclkGpio = board::pins::Si4684Sclk,
        .rstbGpio = board::pins::Si4684Rstb,
        .intbGpio = board::pins::Si4684Intb,
    },
    gImages.romPatch(),
    gImages.dabFirmware(),
    gImages.fmFirmware());
si4684::Si4684Tuner gSi4684Tuner(gSi4684);

adau1701::EmbeddedDspProgramSource gEmbeddedDspProgram;
adau1701::FlashDspProgramSource gFlashDspProgram;
adau1701::FallbackDspProgramSource gDspProgramSource(
    gFlashDspProgram, gEmbeddedDspProgram);

adau1701::Adau1701Driver gAdau1701(
    adau1701::Adau1701Pins{
        .i2cSda = board::pins::Adau1701Sda,
        .i2cScl = board::pins::Adau1701Scl,
        .resetGpio = board::pins::Adau1701Reset,
        .i2cAddr7 = board::pins::Adau1701Addr,
    },
    gDspProgramSource);
adau1701::Adau1701Dsp gAdau1701Dsp(gAdau1701);
secure_store::NvsAudioProfileStore gAudioStore;
audio::AudioService gAudioService(gAdau1701Dsp, &gAudioStore);

bt1035::Bt1035Driver gBt1035(
    bt1035::Bt1035Pins{
        .uartTx = board::pins::Bt1035UartTx,
        .uartRx = board::pins::Bt1035UartRx,
        .rtsGpio = board::pins::Bt1035Rts,
        .ctsGpio = board::pins::Bt1035Cts,
        .resetGpio = board::pins::Bt1035Reset,
        .sysCtlGpio = board::pins::Bt1035SysCtl,
    });

core::DeviceIdentity gDeviceIdentity = core::DeviceIdentity::unknown();
bool gReady = false;
} // namespace

std::expected<void, HardwareBootError> HardwareBootstrap::boot()
{
    if (gReady) {
        return {};
    }

    if (auto tunerResult = gSi4684.boot(si4684::Si4684Band::Dab); !tunerResult) {
        ESP_LOGE(kTag, "Si4684 boot failed");
        return std::unexpected(HardwareBootError::Si4684BootFailed);
    }

    if (!gAdau1701.isBooted()) {
        auto dspResult = gAdau1701.boot();
        if (!dspResult) {
            ESP_LOGE(kTag, "ADAU1701 boot failed");
            return std::unexpected(HardwareBootError::Adau1701BootFailed);
        }
    }

    const auto* busHandle =
        static_cast<i2c_master_bus_handle_t>(gAdau1701.i2cBusHandle());
    eeprom24aa::Eeprom24aa eeprom(busHandle,
                                  static_cast<std::uint8_t>(
                                      board::pins::Eeprom24aaAddr));
    if (auto identity = eeprom.readDeviceIdentity(); identity) {
        gDeviceIdentity = std::move(*identity);
        ESP_LOGI(kTag, "unit serial %.*s",
                 static_cast<int>(gDeviceIdentity.serialNumber().size()),
                 gDeviceIdentity.serialNumber().data());
    } else {
        gDeviceIdentity = core::DeviceIdentity::unknown();
        ESP_LOGW(kTag, "EUI-48 read failed — using fallback identity");
    }

    if (auto audioResult = gAudioService.loadAndApply(); !audioResult) {
        ESP_LOGW(kTag, "ADAU1701 profile apply failed");
    }

    if (auto btResult = gBt1035.boot(); !btResult) {
        ESP_LOGE(kTag, "BT1035 boot failed");
        return std::unexpected(HardwareBootError::Bt1035BootFailed);
    }

    if (auto nameResult =
            gBt1035.setDeviceName(gDeviceIdentity.bluetoothName()); !nameResult) {
        ESP_LOGW(kTag, "BT1035 device name set failed");
    }

    if (auto reconnectResult = gBt1035.setAutoReconnect(3U); !reconnectResult) {
        ESP_LOGW(kTag, "BT1035 auto-reconnect set failed");
    }

    gReady = true;
    ESP_LOGI(kTag, "companion chips ready");
    return {};
}

si4684::Si4684Tuner& HardwareBootstrap::si4684Tuner()
{
    return gSi4684Tuner;
}

audio::AudioService& HardwareBootstrap::audioService()
{
    return gAudioService;
}

core::CompanionChipStatus HardwareBootstrap::companionChipStatus() noexcept
{
    return core::CompanionChipStatus{
        .si4684Ready = gSi4684.isBooted(),
        .adau1701Ready = gAdau1701.isBooted(),
        .bt1035Ready = gBt1035.isBooted(),
    };
}

bt1035::Bt1035Driver& HardwareBootstrap::bt1035Driver()
{
    return gBt1035;
}

const core::DeviceIdentity& HardwareBootstrap::deviceIdentity() noexcept
{
    return gDeviceIdentity;
}

} // namespace hardware
