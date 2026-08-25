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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
        .resetGpio = board::pins::Bt1035Reset,
        .sysCtlGpio = board::pins::Bt1035SysCtl,
        .ctsGpio = board::pins::Bt1035Cts,
        .rtsGpio = board::pins::Bt1035Rts,
    });

core::DeviceIdentity gDeviceIdentity = core::DeviceIdentity::unknown();
std::optional<std::uint8_t> gFmAntCapCalibration;
std::optional<std::uint8_t> gDabAntCapCalibration;
bool gReady = false;

/**
 * @brief    makeEeprom — construct a transient EEPROM handle onto the
 *           shared I2C bus, matching the one built inline in boot().
 */
[[nodiscard]] eeprom24aa::Eeprom24aa makeEeprom()
{
    auto* busHandle =
        static_cast<i2c_master_bus_handle_t>(gAdau1701.i2cBusHandle());
    return eeprom24aa::Eeprom24aa(
        busHandle,
        static_cast<std::uint8_t>(board::pins::Eeprom24aaAddr));
}

/**
 * @brief    applyBt1035PostBootSetup — device name + auto-reconnect, run
 *           once after any successful BT1035 boot (first attempt or a
 *           later background retry).
 */
void applyBt1035PostBootSetup()
{
    if (auto nameResult = gBt1035.setDeviceName(gDeviceIdentity.bluetoothName());
        !nameResult) {
        ESP_LOGW(kTag, "BT1035 device name set failed");
    }
    if (auto reconnectResult = gBt1035.setAutoReconnect(3U); !reconnectResult) {
        ESP_LOGW(kTag, "BT1035 auto-reconnect set failed");
    }
}

/**
 * @brief    bt1035RetryTask — keep retrying Bt1035Driver::boot() in the
 *           background after the initial boot() attempt fails.
 *
 * @dname    bt1035RetryTask
 * @pubstate loops gBt1035.boot() with no artificial delay between
 *           attempts — each call already blocks for tens of seconds
 *           (kBootBannerWaitMs's banner wait, times kBootAttempts), so no
 *           extra backoff is added on top. Exits once boot() succeeds.
 *
 * Why: BT1035 boot failure has been observed to be intermittent on
 * identical, correctly-wired, correctly-powered hardware — the same
 * physical module has booted successfully and failed silently across
 * different attempts in the same session, with the crystal oscillator
 * inside the (sealed, non-serviceable) module the leading suspect. Since
 * the fault clears on a later attempt rather than needing repair, retrying
 * indefinitely in the background turns a permanent-until-manual-reboot
 * failure into a bounded, self-recovering delay.
 *
 * @author   Michele Bigi
 * @date     2026-08-21
 */
void bt1035RetryTask(void* /*arg*/)
{
    ESP_LOGW(kTag, "BT1035 background retry started");
    while (true) {
        if (auto result = gBt1035.boot(); result) {
            applyBt1035PostBootSetup();
            ESP_LOGI(kTag, "BT1035 background retry succeeded");
            break;
        }
        ESP_LOGW(kTag, "BT1035 background retry attempt failed, trying again");
    }
    vTaskDelete(nullptr);
}
} // namespace

std::expected<void, HardwareBootError> HardwareBootstrap::boot()
{
    if (gReady) {
        return {};
    }

    // ADAU1701 boots first (independent I2C/SPI chips, no cross-dependency)
    // so its I2C bus is available for the EEPROM read below, needed to load
    // the Si4684 crystal trim before Si4684 itself boots.
    if (!gAdau1701.isBooted()) {
        auto dspResult = gAdau1701.boot();
        if (!dspResult) {
            ESP_LOGE(kTag, "ADAU1701 boot failed");
            return std::unexpected(HardwareBootError::Adau1701BootFailed);
        }
    }

    eeprom24aa::Eeprom24aa eeprom = makeEeprom();

    // Fallback defaults (2026-08-23): ctun=0, xtalFreqHz=19199750 -- the
    // compiled-in defaults (ctun=31, xtal=19200000 nominal) were never
    // measured against this board's actual crystal (Abracon
    // ABM8-19.200MHZ-10-1-U-T, CL=10pF per part number, plus two external
    // 15pF load caps per the schematic). CTUN=0 was found audibly best via
    // A/B listening (0/5/31), then xtalFreqHz was trimmed properly using
    // the chip's own FM_RSQ FREQOFF measurement
    // (tools/si4684_xtal_calibration.py) against two real, GPS-locked
    // broadcast carriers 87.6/105.1 MHz -- converged to -3 to -4 ppm
    // residual on both, down from +70 ppm uncorrected. Used only when the
    // EEPROM has never been calibrated (or every board would need the same
    // physical crystal tolerance, which isn't guaranteed). See POST
    // /api/tuner/xtal-calibrate to re-trim live, and POST it again to
    // persist -- see saveXtalCalibration() below.
    std::uint8_t xtalIbias = 72U;
    std::uint8_t xtalCtun = 0U;
    std::uint32_t xtalFreqHz = 19199750U;
    if (auto xtal = eeprom.readXtalCalibration(); xtal) {
        if (*xtal) {
            xtalIbias = (*xtal)->ibias;
            xtalCtun = (*xtal)->ctun;
            xtalFreqHz = (*xtal)->xtalFreqHz;
            ESP_LOGI(kTag,
                     "Xtal calibration loaded: ibias=%u ctun=%u "
                     "xtal_freq_hz=%lu",
                     static_cast<unsigned>(xtalIbias),
                     static_cast<unsigned>(xtalCtun),
                     static_cast<unsigned long>(xtalFreqHz));
        } else {
            ESP_LOGI(kTag,
                     "Xtal not calibrated — using compiled-in defaults");
        }
    } else {
        ESP_LOGW(kTag, "Xtal calibration read failed — using compiled-in "
                       "defaults");
    }

    if (auto tunerResult = gSi4684.boot(si4684::Si4684Band::Dab, xtalIbias,
                                        xtalCtun, xtalFreqHz);
        !tunerResult) {
        ESP_LOGE(kTag, "Si4684 boot failed: error %d", static_cast<int>(tunerResult.error()));
        return std::unexpected(HardwareBootError::Si4684BootFailed);
    }

    if (auto identity = eeprom.readDeviceIdentity(); identity) {
        gDeviceIdentity = std::move(*identity);
        ESP_LOGI(kTag, "unit serial %.*s",
                 static_cast<int>(gDeviceIdentity.serialNumber().size()),
                 gDeviceIdentity.serialNumber().data());
    } else {
        gDeviceIdentity = core::DeviceIdentity::unknown();
        ESP_LOGW(kTag, "EUI-48 read failed — using fallback identity");
    }

    if (auto antCap = eeprom.readFmAntCap(); antCap) {
        gFmAntCapCalibration = *antCap;
        if (gFmAntCapCalibration) {
            ESP_LOGI(kTag, "FM ANTCAP calibration loaded: %u",
                     static_cast<unsigned>(*gFmAntCapCalibration));
        } else {
            ESP_LOGI(kTag, "FM ANTCAP not calibrated — using chip auto-tune");
        }
    } else {
        ESP_LOGW(kTag, "FM ANTCAP calibration read failed — using chip "
                       "auto-tune");
    }

    if (auto antCap = eeprom.readDabAntCap(); antCap) {
        gDabAntCapCalibration = *antCap;
        if (gDabAntCapCalibration) {
            ESP_LOGI(kTag, "DAB ANTCAP calibration loaded: %u",
                     static_cast<unsigned>(*gDabAntCapCalibration));
        } else {
            ESP_LOGI(kTag, "DAB ANTCAP not calibrated — using chip auto-tune");
        }
    } else {
        ESP_LOGW(kTag, "DAB ANTCAP calibration read failed — using chip "
                       "auto-tune");
    }

    if (auto audioResult = gAudioService.loadAndApply(); !audioResult) {
        ESP_LOGW(kTag, "ADAU1701 profile apply failed");
    }
    if (auto radioMix = gAudioService.applyRadioFirstMix(false); !radioMix) {
        ESP_LOGW(kTag, "ADAU1701 radio-first mix failed");
    } else {
        ESP_LOGI(kTag, "ADAU1701 Si4684 input routed (radio-first mix)");
    }

    if (auto btResult = gBt1035.boot(); !btResult) {
        ESP_LOGE(kTag, "BT1035 boot failed — continuing without Bluetooth, "
                       "retrying in background");
        if (xTaskCreate(bt1035RetryTask, "bt1035_retry", 4096, nullptr, 3,
                        nullptr)
            != pdPASS) {
            ESP_LOGW(kTag, "BT1035 background retry task create failed");
        }
    } else {
        applyBt1035PostBootSetup();
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

std::optional<std::uint8_t> HardwareBootstrap::fmAntCapCalibration() noexcept
{
    return gFmAntCapCalibration;
}

bool HardwareBootstrap::saveFmAntCapCalibration(std::uint8_t antCap)
{
    eeprom24aa::Eeprom24aa eeprom = makeEeprom();
    if (auto written = eeprom.writeFmAntCap(antCap); !written) {
        ESP_LOGW(kTag, "FM ANTCAP calibration write failed");
        return false;
    }
    gFmAntCapCalibration = antCap;
    ESP_LOGI(kTag, "FM ANTCAP calibration saved: %u",
             static_cast<unsigned>(antCap));
    return true;
}

std::optional<std::uint8_t> HardwareBootstrap::dabAntCapCalibration() noexcept
{
    return gDabAntCapCalibration;
}

bool HardwareBootstrap::saveDabAntCapCalibration(std::uint8_t antCap)
{
    eeprom24aa::Eeprom24aa eeprom = makeEeprom();
    if (auto written = eeprom.writeDabAntCap(antCap); !written) {
        ESP_LOGW(kTag, "DAB ANTCAP calibration write failed");
        return false;
    }
    gDabAntCapCalibration = antCap;
    ESP_LOGI(kTag, "DAB ANTCAP calibration saved: %u",
             static_cast<unsigned>(antCap));
    return true;
}

bool HardwareBootstrap::saveXtalCalibration(std::uint8_t ibias,
                                            std::uint8_t ctun,
                                            std::uint32_t xtalFreqHz)
{
    eeprom24aa::Eeprom24aa eeprom = makeEeprom();
    const eeprom24aa::XtalCalibration calibration{
        .ibias = ibias, .ctun = ctun, .xtalFreqHz = xtalFreqHz};
    if (auto written = eeprom.writeXtalCalibration(calibration); !written) {
        ESP_LOGW(kTag, "Xtal calibration write failed");
        return false;
    }
    ESP_LOGI(kTag,
             "Xtal calibration saved: ibias=%u ctun=%u xtal_freq_hz=%lu",
             static_cast<unsigned>(ibias), static_cast<unsigned>(ctun),
             static_cast<unsigned long>(xtalFreqHz));
    return true;
}

} // namespace hardware
