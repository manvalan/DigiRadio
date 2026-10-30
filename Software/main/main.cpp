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

#include "alarm_clock.hpp"
#include "board_pins.hpp"
#include "hardware_bootstrap.hpp"
#include "audio/AudioService.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "integration/IntegrationService.hpp"
#include "net/NetBootstrap.hpp"
#include "net/SetupWebServer.hpp"
#include "secure_store/NvsPlatformInit.hpp"
#include "ota/OtaService.hpp"
#include "secure_store/NvsSecureStore.hpp"
#include "si4684/Si4684Tuner.hpp"
#include "station/StationService.hpp"
#include "dab_slideshow.hpp"
#include "tuner/AfFollowService.hpp"
#include "tuner/SpectrumScanService.hpp"
#include "tuner/TunerService.hpp"
#include "antenna_calibration.hpp"
#include "audio_routing.hpp"
#include "esp32_i2s_sink.hpp"
#include "phone_pcm_server.hpp"
#include "phone_stream.hpp"
#include "system_diag.hpp"
#include "web_radio_stream.hpp"
#include "webradio/WebRadioService.hpp"

#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_bt.h"
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

#if CONFIG_ESP32_I2S_TEST_TONE
#include "esp32_i2s_test_tone.hpp"
#endif

namespace {

constexpr char kTag[] = "digiradio";
constexpr TickType_t kHeartbeatPeriod = pdMS_TO_TICKS(60000);
/** RDS AF follow poll period (the iOS app used to poll every 3-4 s). */
/** DAB data drain period; the tuner's DSRV queue is small. */
constexpr TickType_t kDabDataPeriod = pdMS_TO_TICKS(150);
/** AF follow tick: every 20 drains, i.e. every 3 s as before. */
constexpr unsigned kDrainsPerAfTick = 20U;

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
        // Heap and stack low-water marks once a minute: the resources that
        // run out quietly (see also GET /api/health "system").
        system_diag::logSummary();
        vTaskDelay(kHeartbeatPeriod);
    }
}

void sleepMs(std::uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief    afFollowTask — drain DAB data every 150 ms and run AF/DAB
 *           following every 3 s.
 *
 * @dname    afFollowTask
 * @param    arg  tuner::AfFollowService* (static lifetime).
 * @pubstate dab_slideshow::poll() every kDabDataPeriod and
 *           AfFollowService::tick() every kDrainsPerAfTick drains, with or
 *           without an app.
 *
 * @author   Michele Bigi
 * @date     2026-10-21
 */
[[noreturn]] void afFollowTask(void* arg)
{
    auto* follow = static_cast<tuner::AfFollowService*>(arg);
    UBaseType_t lowWater = ~0U;
    for (unsigned n = 0U;; ++n) {
        dab_slideshow::poll();
        if (n % kDrainsPerAfTick == 0U) {
            follow->tick(
                static_cast<std::uint32_t>(esp_timer_get_time() / 1000));
        }
        // TunerStatus copies, RDS buffers and hunts are stack-heavy: an
        // undersized stack overflowed here in 0.9.6. Log new lows.
        const UBaseType_t free = uxTaskGetStackHighWaterMark(nullptr);
        if (free < lowWater) {
            lowWater = free;
            ESP_LOGI(kTag, "af_follow stack free low-water: %u bytes",
                     static_cast<unsigned>(free));
        }
        vTaskDelay(kDabDataPeriod);
    }
}

#if CONFIG_ESP32_I2S_TEST_TONE
[[noreturn]] void i2sTestToneTask(void* arg)
{
    (void)arg;
    esp32_i2s_test_tone::run(440.0F);
}
#endif

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

    // NVS must be initialized before HardwareBootstrap::boot(), which
    // internally calls AudioService::loadAndApply() to restore the saved
    // mixer/EQ/master-volume profile (2026-08-24 root-cause fix): this used
    // to run in the other order, so every nvs_open() inside
    // NvsAudioProfileStore::hasProfile()/loadProfile() failed with
    // ESP_ERR_NVS_NOT_INITIALIZED (0x1101, silently swallowed as "no saved
    // profile") on every single boot -- the audio profile was never
    // actually restored, no matter how many times it was saved via
    // PUT /api/audio/profile. nvs_flash_init() itself has no hardware
    // dependency (pure flash-partition access), so moving it first is safe.
    if (auto nvsResult = secure_store::initEncryptedStorage(); !nvsResult) {
        ESP_LOGE(kTag, "NVS init failed — halting");
        return;
    }

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

    audio_routing::init(hardware::HardwareBootstrap::audioService());

    static bluetooth::BluetoothService bluetoothService(
        hardware::HardwareBootstrap::bt1035Driver(), store);

    static ota::OtaService otaService;

    static webradio::WebRadioService webRadioService(store);

    if (!esp32_i2s_sink::open()) {
        ESP_LOGW(kTag, "shared I2S sink open failed — web radio and phone "
                       "streaming will be unavailable");
    }

#if CONFIG_ESP32_I2S_TEST_TONE
    if (xTaskCreate(i2sTestToneTask, "i2s_test_tone", 4096, nullptr, 4,
                    nullptr) != pdPASS) {
        ESP_LOGW(kTag, "ESP32 I2S test tone task create failed");
    }
#endif

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

    net::setSystemDiagnosticsProvider(&system_diag::json);
    system_diag::watchTask("httpd", xTaskGetHandle("httpd"));

    TaskHandle_t handle = nullptr;
    // 0.10.0 measured 1.9 KB left of 4 KB once logSummary() ran.
    if (xTaskCreate(heartbeatTask, "heartbeat", 6144, nullptr, 5, &handle)
        != pdPASS) {
        ESP_LOGE(kTag, "heartbeat task create failed");
        return;
    }
    system_diag::watchTask("heartbeat", handle);

    bluetoothService.startupReconnect();

    static tuner::AfFollowService afFollow(tunerService, sleepMs);
    static tuner::SpectrumScanService spectrum(tunerService);
    net::setSpectrumScanService(&spectrum);
    net::setPresetService(hardware::HardwareBootstrap::presetService());

    if (!dab_slideshow::start(tunerService)) {
        ESP_LOGW(kTag, "slide flash unavailable: DLS only, no slideshow");
    }
    // AF follow plus the DAB data drain (0.12.1: one task instead of two).
    // The drain peaked at 5.8 KB in 0.12.0's own 8 KB task, AF at ~2 KB;
    // they run one after the other, so 10 KB keeps a 4 KB margin.
    if (xTaskCreate(afFollowTask, "af_follow", 10240, &afFollow, 3, &handle)
        != pdPASS) {
        ESP_LOGW(kTag, "AF follow task create failed");
    } else {
        system_diag::watchTask("af_follow", handle);
    }

    // 0.12.3 measured 14 KB of 16 KB never used: the decoder state is on
    // the heap, not on this stack.
    if (xTaskCreate(web_radio_stream::run, "web_radio", 8192,
                    &webRadioService, 4, &handle) != pdPASS) {
        ESP_LOGW(kTag, "web radio stream task create failed");
    } else {
        system_diag::watchTask("web_radio", handle);
    }

    if (!phone_pcm_server::start()) {
        ESP_LOGW(kTag, "phone PCM server task create failed");
    }

    static const alarm_clock::Services alarmServices{
        .audio = hardware::HardwareBootstrap::audioService(),
        .integration = integration,
        .webRadio = webRadioService,
        .bluetooth = bluetoothService,
    };
    if (!alarm_clock::start(alarmServices)) {
        ESP_LOGW(kTag, "alarm task create failed");
    }

    // Only now, with every task started, does the probation clock run: an
    // image that crashes in any of them is rolled back by the bootloader.
    system_diag::confirmOtaAfterProbation();

    if (net.state() == net::NetState::StaConnected) {
        // BLE is only used for Wi-Fi provisioning, which runs in setup mode
        // (a fresh boot); in STA mode give its controller and NimBLE
        // memory to the heap for the web radio (fw 0.12.4).
        if (esp_bt_mem_release(ESP_BT_MODE_BLE) != ESP_OK) {
            ESP_LOGW(kTag, "BLE memory not released");
        }
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
