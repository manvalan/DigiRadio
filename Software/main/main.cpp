/**
 * @file    main.cpp
 * @brief   DigiRadio imperative shell entry point (app_main).
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "board_pins.hpp"
#include "hardware_bootstrap.hpp"
#include "audio/AudioService.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "integration/IntegrationService.hpp"
#include "net/NetBootstrap.hpp"
#include "secure_store/NvsPlatformInit.hpp"
#include "ota/OtaService.hpp"
#include "secure_store/NvsSecureStore.hpp"
#include "si4684/Si4684Tuner.hpp"
#include "station/StationService.hpp"
#include "tuner/TunerService.hpp"
#include "antenna_calibration.hpp"
#include "esp32_i2s_sink.hpp"
#include "phone_stream.hpp"
#include "web_radio_stream.hpp"
#include "webradio/WebRadioService.hpp"

#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

#if CONFIG_TEST_FIRMWARE
#include "test_firmware.hpp"
#endif

#if CONFIG_I2S_SDATA_PROBE
#include "i2s_sdata_probe.hpp"
#endif

namespace {

constexpr char kTag[] = "digiradio";
constexpr TickType_t kHeartbeatPeriod = pdMS_TO_TICKS(5000);

/**
 * @brief    logGpioClockFrequency — one-shot edge-count frequency probe.
 *
 * Uses the ESP32-S3 PCNT peripheral to count rising edges on an input GPIO
 * over a fixed window and log the resulting frequency, so BCLK/LRCLK on the
 * ADAU1701 -> ESP32 I2S bus (board::pins::I2sBclk/I2sLrclk, shared clock
 * domain with the ADAU -> BT1035 link, see docs/manual/ch-bt1035.tex) can be
 * verified without an oscilloscope. Read-only: the PCNT channel only listens
 * on the pad, so it does not conflict with the I2S peripheral also reading
 * the same GPIO later (ESP32 GPIO matrix fans one input pad out to multiple
 * peripherals). windowMs must stay short enough that expected edge count
 * does not exceed the PCNT hardware limit (signed 16-bit, so < 32767).
 */
void logGpioClockFrequency(const char* label, int gpio, int windowMs)
{
    pcnt_unit_config_t unitConfig{};
    unitConfig.low_limit = -1;
    unitConfig.high_limit = 30000;
    pcnt_unit_handle_t unit = nullptr;
    if (pcnt_new_unit(&unitConfig, &unit) != ESP_OK) {
        ESP_LOGW(kTag, "%s clock probe: pcnt_new_unit failed", label);
        return;
    }

    pcnt_chan_config_t chanConfig{};
    chanConfig.edge_gpio_num = gpio;
    chanConfig.level_gpio_num = -1;
    pcnt_channel_handle_t chan = nullptr;
    if (pcnt_new_channel(unit, &chanConfig, &chan) != ESP_OK) {
        ESP_LOGW(kTag, "%s clock probe: pcnt_new_channel failed (GPIO%d)",
                 label, gpio);
        pcnt_del_unit(unit);
        return;
    }
    pcnt_channel_set_edge_action(chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                 PCNT_CHANNEL_EDGE_ACTION_HOLD);

    pcnt_unit_enable(unit);
    pcnt_unit_clear_count(unit);
    pcnt_unit_start(unit);
    // Busy-wait on esp_timer instead of vTaskDelay: at the default 100 Hz
    // FreeRTOS tick rate, a short requested window (e.g. 5-10 ms for BCLK)
    // rounds to the nearest 10 ms tick, which is a huge relative error at
    // MHz-range signals. Measuring the *actual* elapsed time afterwards
    // makes the frequency calculation correct regardless of overshoot.
    const std::int64_t startUs = esp_timer_get_time();
    const std::int64_t targetUs = static_cast<std::int64_t>(windowMs) * 1000;
    while (esp_timer_get_time() - startUs < targetUs) {
        // intentionally tight: window is a few ms to a few hundred ms,
        // far under any watchdog timeout.
    }
    pcnt_unit_stop(unit);
    const std::int64_t elapsedUs = esp_timer_get_time() - startUs;

    int count = 0;
    pcnt_unit_get_count(unit, &count);

    pcnt_del_channel(chan);
    pcnt_unit_disable(unit);
    pcnt_del_unit(unit);

    const long freqHz = elapsedUs > 0
        ? static_cast<long>((static_cast<std::int64_t>(count) * 1000000LL)
                             / elapsedUs)
        : 0;
    const bool nearSaturation = count >= (unitConfig.high_limit * 9) / 10;
    ESP_LOGI(kTag, "%s clock probe: GPIO%d = %ld edges / %lld us -> ~%ld Hz%s",
             label, gpio, static_cast<long>(count),
             static_cast<long long>(elapsedUs), freqHz,
             nearSaturation ? " (near PCNT limit — window too long for this "
                              "clock, re-run with a shorter windowMs)"
                            : "");
}

void heartbeatTask(void* arg)
{
    (void)arg;
    while (true) {
        ESP_LOGI(kTag, "heartbeat");
        vTaskDelay(kHeartbeatPeriod);
    }
}

} // namespace

/**
 * @brief    app_main — ESP-IDF entry point for DigiRadio firmware.
 *
 * @dname    app_main
 * @pubstate boots companion chips, integration layer, network, and heartbeat.
 *
 * @author   Michele Bigi
 * @date     2026-07-06
 */
extern "C" void app_main()
{
    ESP_LOGI(kTag, "DigiRadio firmware boot");

    auto hwResult = hardware::HardwareBootstrap::boot();
    if (!hwResult) {
        ESP_LOGE(kTag, "companion chip boot failed — halting");
        return;
    }

    ESP_LOGI(kTag, "==== ADAU1701 I2S clock probe (no scope needed) ====");
    // 500 ms: at 44.1-48 kHz this stays under the 16-bit PCNT limit
    // (~24000 counts) while keeping tick/overhead error under ~1%.
    logGpioClockFrequency("LRCLK", board::pins::I2sLrclk, 500);
    // 10 ms: BCLK is in the MHz range (Nx LRCLK), so the window must be
    // short enough that expected edges stay under the PCNT limit.
    logGpioClockFrequency("BCLK", board::pins::I2sBclk, 10);

#if CONFIG_I2S_SDATA_PROBE
    i2s_sdata_probe::captureAndLog(500);
#endif

#if CONFIG_TEST_FIRMWARE
    test_firmware::runTestFirmware();
    return;
#endif

    if (auto nvsResult = secure_store::initEncryptedStorage(); !nvsResult) {
        ESP_LOGE(kTag, "NVS init failed — halting");
        return;
    }

    static secure_store::NvsSecureStore store;

    static tuner::TunerService tunerService(
        hardware::HardwareBootstrap::si4684Tuner());
    if (auto antCap = hardware::HardwareBootstrap::fmAntCapCalibration();
        antCap) {
        tunerService.setDefaultFmAntCap(*antCap);
    }
    if (auto antCap = hardware::HardwareBootstrap::dabAntCapCalibration();
        antCap) {
        tunerService.setDefaultDabAntCap(*antCap);
    }

    static station::StationService stationService(store, tunerService);

    static integration::IntegrationService integration(
        store,
        tunerService,
        hardware::HardwareBootstrap::audioService(),
        stationService);

    if (auto started = integration.startup(); !started) {
        ESP_LOGW(kTag, "integration startup failed — continuing without recall");
    }

    static bluetooth::BluetoothService bluetoothService(
        hardware::HardwareBootstrap::bt1035Driver(), store);

    static ota::OtaService otaService;

    static webradio::WebRadioService webRadioService(store);

    if (!esp32_i2s_sink::open()) {
        ESP_LOGW(kTag, "shared I2S sink open failed — web radio and phone "
                       "streaming will be unavailable");
    }

    auto netResult = net::NetBootstrap::start(
        store,
        tunerService,
        hardware::HardwareBootstrap::audioService(),
        bluetoothService,
        stationService,
        integration,
        otaService,
        webRadioService,
        phone_stream::sink(),
        antenna_calibration::bridge(),
        hardware::HardwareBootstrap::companionChipStatus(),
        hardware::HardwareBootstrap::deviceIdentity());
    if (!netResult) {
        ESP_LOGE(kTag, "network bootstrap failed");
        return;
    }

    static net::NetBootstrap net = std::move(netResult.value());

    if (auto confirmed = ota::OtaService::confirmBoot(); !confirmed) {
        ESP_LOGW(kTag, "OTA rollback confirm failed");
    }

    if (xTaskCreate(heartbeatTask, "heartbeat", 4096, nullptr, 5, nullptr)
        != pdPASS) {
        ESP_LOGE(kTag, "heartbeat task create failed");
        return;
    }

    bluetoothService.startupReconnect();

    if (xTaskCreate(web_radio_stream::run, "web_radio", 16384,
                    &webRadioService, 4, nullptr) != pdPASS) {
        ESP_LOGW(kTag, "web radio stream task create failed");
    }

    if (net.state() == net::NetState::StaConnected) {
        const auto& identity = hardware::HardwareBootstrap::deviceIdentity();
        ESP_LOGI(kTag,
                 "running in STA mode — open http://%.*s.local/ "
                 "(or check serial for IP)",
                 static_cast<int>(identity.hostname().size()),
                 identity.hostname().data());
    } else {
        const auto& identity = hardware::HardwareBootstrap::deviceIdentity();
        ESP_LOGI(kTag,
                 "running in setup mode — join %.*s, "
                 "open http://192.168.4.1/",
                 static_cast<int>(identity.softApSsid().size()),
                 identity.softApSsid().data());
    }
}
