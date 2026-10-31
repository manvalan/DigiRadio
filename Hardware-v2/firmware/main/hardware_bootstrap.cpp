/**
 * @file    hardware_bootstrap.cpp
 * @brief   HardwareBootstrap for the v2 board.
 *
 * Same interface as the v1 bootstrap (Software/main/hardware_bootstrap.hpp);
 * the chips and their order differ.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "hardware_bootstrap.hpp"

#include "adau1452/Adau1452Driver.hpp"
#include "adau1452/Adau1452Dsp.hpp"
#include "adau1452/Adau1452Eeprom.hpp"
#include "adau1452/Adau1452Images.hpp"
#include "adau1452/Adau1452Store.hpp"
#include "audio/AudioService.hpp"
#include "audio/PresetService.hpp"
#include "board_pins.hpp"
#include "board_v2.hpp"
#include "bt1035/Bt1035Driver.hpp"
#include "core/DeviceIdentity.hpp"
#include "driver/i2c_master.h"
#include "eeprom24aa/Eeprom24aa.hpp"
#include "eeprom24aa/EepromAudioPresetStore.hpp"
#include "secure_store/NvsAudioPresetStore.hpp"
#include "secure_store/NvsAudioProfileStore.hpp"
#include "si4684/Si4684Band.hpp"
#include "si4684/Si4684Driver.hpp"
#include "si4684/Si4684EmbeddedImages.hpp"
#include "si4684/Si4684Tuner.hpp"

#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <optional>
#include <span>
#include <vector>

namespace hardware {

namespace {
constexpr char kTag[] = "hw_boot";

#if !CONFIG_SI4684_EMBEDDED_IMAGES
#error "v2 streams the Si4684 patch from the app and fills U7 from its images"
#endif
si4684::Si4684EmbeddedImages gImages;
si4684::Si4684Driver gSi4684(
    si4684::Si4684Pins{
        .spiHost = SPI2_HOST,
        .csGpio = board::pins::Si4684Cs,
        .misoGpio = board::pins::SpiMiso,
        .mosiGpio = board::pins::SpiMosi,
        .sclkGpio = board::pins::SpiSclk,
        .rstbGpio = board::pins::Si4684Rstb,
        .intbGpio = board::pins::Si4684Intb,
        .sharedBus = true,
    },
    gImages.romPatch(),
    gImages.dabFirmware(),
    gImages.fmFirmware());
si4684::Si4684Tuner gSi4684Tuner(gSi4684);

/** U7 is a 2 MB W25Q16JW; each image is under 512 KiB + 32 KiB. */
constexpr si4684::Si4684NvLayout kNvLayout{.dabOffset = 0x000000U,
                                           .fmOffset = 0x100000U};
const char* gNvFlashState = "unknown";

adau1452::store::ProgramSource gDspProgram;
adau1452::Adau1452Driver gAdau1452(
    adau1452::Adau1452Pins{
        .spiHost = SPI2_HOST,
        .csGpio = board::pins::AdauCs,
        .resetGpio = board::pins::AdauReset,
        .selfBootGpio = board::pins::AdauSelfBoot,
    },
    gDspProgram);
adau1452::Adau1452Dsp gAdau1452Dsp(gAdau1452);
secure_store::NvsAudioProfileStore gAudioStore;
audio::AudioService gAudioService(gAdau1452Dsp, &gAudioStore);
secure_store::NvsAudioPresetStore gPresetNvs;
std::optional<eeprom24aa::Eeprom24aa> gPresetEeprom;
std::optional<eeprom24aa::EepromAudioPresetStore> gPresetBackup;
std::optional<audio::PresetService> gPresets;

bt1035::Bt1035Driver gBt1058(
    bt1035::Bt1035Pins{
        .uartTx = board::pins::BtUartTx,
        .uartRx = board::pins::BtUartRx,
        .resetGpio = board::pins::BtReset,
        .sysCtlGpio = board::pins::BtSysCtrl,
        .ctsGpio = -1,
        .rtsGpio = -1,
    });

core::DeviceIdentity gDeviceIdentity = core::DeviceIdentity::unknown();
std::optional<std::uint8_t> gFmAntCapCalibration;
std::optional<std::uint8_t> gDabAntCapCalibration;
bool gReady = false;

/**
 * @brief    makeEeprom — construct a transient EEPROM handle onto the
 *           I2C bus. The 24AA025E64 keeps its factory EUI-64 at 0xF8-0xFF, so the
 *           driver's 0xFA read returns its last six bytes: still unique.
 */
[[nodiscard]] eeprom24aa::Eeprom24aa makeEeprom()
{
    return eeprom24aa::Eeprom24aa(
        board_v2::i2cBus(),
        static_cast<std::uint8_t>(board::pins::EepromAddr));
}

/**
 * @brief    applyBt1035PostBootSetup — device name + auto-reconnect, run
 *           once after any successful BT1058 boot (first attempt or a
 *           later background retry).
 */
void applyBt1035PostBootSetup()
{
    if (auto nameResult = gBt1058.setDeviceName(gDeviceIdentity.bluetoothName());
        !nameResult) {
        ESP_LOGW(kTag, "BT1058 device name set failed");
    }
    if (auto reconnectResult = gBt1058.setAutoReconnect(3U); !reconnectResult) {
        ESP_LOGW(kTag, "BT1058 auto-reconnect set failed");
    }
}

/**
 * @brief    bt1035RetryTask — keep retrying Bt1035Driver::boot() in the
 *           background after the initial boot() attempt fails.
 *
 * @dname    bt1035RetryTask
 * @pubstate loops gBt1058.boot() with no artificial delay between
 *           attempts — each call already blocks for tens of seconds
 *           (kBootBannerWaitMs's banner wait, times kBootAttempts), so no
 *           extra backoff is added on top. Exits once boot() succeeds.
 *
 * Why: BT1058 boot failure has been observed to be intermittent on
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
/**
 * @brief    bringUpDsp — run the DSP, writing its EEPROM first if needed.
 *
 * Failure is reported, not fatal: the web server and OTA must stay reachable
 * on a board whose DSP program is missing or broken.
 */
void bringUpDsp()
{
    // An uploaded program is a deliberate choice: it wins over the EEPROM.
    const bool useUpload = adau1452::store::hasProgram();
    if (auto first = gAdau1452.boot(useUpload); !first) {
        ESP_LOGE(kTag, "ADAU1452 boot failed: error %d",
                 static_cast<int>(first.error()));
    }
#if CONFIG_DIGIRADIO_DUMP_DSP_EEPROM
    if (!adau1452::dumpSelfBootEeprom(gAdau1452,
                                      CONFIG_DIGIRADIO_DUMP_DSP_EEPROM_BYTES)) {
        ESP_LOGE(kTag, "EEPROM dump failed");
    }
    (void)gAdau1452.boot(useUpload);
    return; // never rewrite the EEPROM being read
#endif
    if (useUpload) {
        return;
    }
    const std::vector<std::uint8_t> uploaded =
        adau1452::store::loadEepromImage();
    const std::span<const std::uint8_t> image =
        uploaded.empty() ? adau1452::selfBootImage()
                         : std::span<const std::uint8_t>(uploaded);
    const adau1452::EepromSync sync =
        adau1452::syncSelfBootEeprom(gAdau1452, image);
    ESP_LOGI(kTag, "ADAU1452 EEPROM sync: %d", static_cast<int>(sync));
    if (sync == adau1452::EepromSync::UpToDate
        || sync == adau1452::EepromSync::NoImage
        || sync == adau1452::EepromSync::NoWriter) {
        return;
    }
    // The writer program replaced the audio program: start over.
    if (auto again = gAdau1452.boot(); !again) {
        ESP_LOGE(kTag, "ADAU1452 boot after EEPROM write failed: error %d",
                 static_cast<int>(again.error()));
    }
}

/**
 * @brief    bringUpNvFlash — make U7 hold the images, boot from it if it does.
 *
 * On any failure the driver keeps streaming the images from the app.
 */
void bringUpNvFlash(std::uint8_t ibias, std::uint8_t ctun, std::uint32_t freqHz)
{
    gNvFlashState = "writing";
    const auto synced = gSi4684.syncNvFlash(kNvLayout, ibias, ctun, freqHz);
    if (!synced) {
        ESP_LOGE(kTag, "Si4684 flash sync failed: error %d — using HOST_LOAD",
                 static_cast<int>(synced.error()));
        gNvFlashState = "missing";
        return;
    }
    if (*synced) {
        ESP_LOGI(kTag, "Si4684 flash written");
    }
    gSi4684.bootFromNvFlash(kNvLayout);
    gNvFlashState = "ready";
}

void bt1035RetryTask(void* /*arg*/)
{
    ESP_LOGW(kTag, "BT1058 background retry started");
    while (true) {
        if (auto result = gBt1058.boot(); result) {
            applyBt1035PostBootSetup();
            ESP_LOGI(kTag, "BT1058 background retry succeeded");
            break;
        }
        ESP_LOGW(kTag, "BT1058 background retry attempt failed, trying again");
    }
    vTaskDelete(nullptr);
}
} // namespace

std::expected<void, HardwareBootError> HardwareBootstrap::boot()
{
    if (gReady) {
        return {};
    }

    if (!board_v2::enableAnalog() || !board_v2::initBuses()) {
        return std::unexpected(HardwareBootError::Adau1701BootFailed);
    }

    // First on the shared SPI bus: until its slave port is in SPI mode the
    // ADAU1452 would take the Si4684 traffic for I2C.
    bringUpDsp();

    eeprom24aa::Eeprom24aa eeprom = makeEeprom();

    // Until tools/si4684_xtal_calibration.py has run on this board: nominal
    // frequency and no added CTUN, since C139/C140 already load the crystal.
    // The v1 trim (19199750 Hz) belongs to that board's crystal and 15 pF caps.
    std::uint8_t xtalIbias = 72U;
    std::uint8_t xtalCtun = 0U;
    std::uint32_t xtalFreqHz = 19200000U;
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

    bringUpNvFlash(xtalIbias, xtalCtun, xtalFreqHz);

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

    // Only fall back to the hardcoded radio-first mix (Si4684 open, ESP32
    // muted) when NO saved profile was restored (2026-08-24 fix): this used
    // to run unconditionally, silently discarding the user's saved
    // mixer/master-volume settings on every single boot regardless of
    // whether loadAndApply() actually found something in NVS.
    const auto audioResult = gAudioService.loadAndApply();
    if (!audioResult) {
        ESP_LOGW(kTag, "ADAU1452 profile apply failed");
    } else if (!*audioResult) {
        if (auto radioMix = gAudioService.applyRadioFirstMix(false);
            !radioMix) {
            ESP_LOGW(kTag, "ADAU1452 radio-first mix failed");
        } else {
            ESP_LOGI(kTag, "ADAU1452 Si4684 input routed (radio-first mix, "
                           "no saved profile)");
        }
    }

    if (!gPresets) {
        gPresetEeprom.emplace(makeEeprom());
        gPresetBackup.emplace(*gPresetEeprom);
        gPresets.emplace(gAudioService, &gPresetNvs, &*gPresetBackup);
        (void)gPresets->load();
        gAudioService.setSourceListener(&*gPresets);
    }

    // Do not block Wi-Fi/HTTP on BT1058 UART bring-up (~25 s per attempt,
    // intermittent silence). The retry task owns boot(); SoftAP setup must
    // not call setEnabled() on the net thread before the web server starts.
    if (gBt1058.isBooted()) {
        applyBt1035PostBootSetup();
    } else if (xTaskCreate(bt1035RetryTask, "bt1035_retry", 4096, nullptr, 3,
                           nullptr)
               != pdPASS) {
        ESP_LOGW(kTag, "BT1058 background retry task create failed");
    } else {
        ESP_LOGW(kTag, "BT1058 boot deferred to background");
    }

    gReady = true;
    ESP_LOGI(kTag, "companion chips ready");
    return {};
}

const char* HardwareBootstrap::imageStoreState() noexcept
{
    return gNvFlashState;
}

si4684::Si4684Tuner& HardwareBootstrap::si4684Tuner()
{
    return gSi4684Tuner;
}

audio::AudioService& HardwareBootstrap::audioService()
{
    return gAudioService;
}

audio::PresetService* HardwareBootstrap::presetService() noexcept
{
    return gPresets ? &*gPresets : nullptr;
}

core::CompanionChipStatus HardwareBootstrap::companionChipStatus() noexcept
{
    return core::CompanionChipStatus{
        .si4684Ready = gSi4684.isBooted(),
        .adau1701Ready = gAdau1452.isBooted(),
        .bt1035Ready = gBt1058.isBooted(),
    };
}

bt1035::Bt1035Driver& HardwareBootstrap::bt1035Driver()
{
    return gBt1058;
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
