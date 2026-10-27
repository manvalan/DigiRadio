/**
 * @file    alarm_clock.cpp
 * @brief   Alarm table (turn on / turn off at set times) and sleep timer.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-10-26
 */

#include "alarm_clock.hpp"

#include "system_diag.hpp"

#include "audio/AudioService.hpp"
#include "bluetooth/BluetoothService.hpp"
#include "core/AlarmJson.hpp"
#include "core/AlarmSchedule.hpp"
#include "integration/IntegrationService.hpp"
#include "net/SetupWebServer.hpp"
#include "webradio/WebRadioService.hpp"

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace alarm_clock {

namespace {

constexpr char kTag[] = "alarm";
constexpr char kNvsNamespace[] = "alarms";
constexpr char kNvsKey[] = "table";
constexpr char kTimeZone[] = "CET-1CEST,M3.5.0,M10.5.0/3"; // Europe/Rome
constexpr std::int64_t kUsPerSecond = 1000000;
/** Earliest plausible SNTP time (2024-01-01): before it the clock is unset. */
constexpr std::time_t kValidEpoch = 1704067200;
/** A master gain this far from the last ramp step means the user moved it. */
constexpr float kUserMoveDb = 0.5F;

/** Wake-up fade-in in progress. */
struct Ramp {
    std::int64_t startUs{0};
    std::uint32_t seconds{0U};
    float targetDb{0.0F};
    float lastDb{0.0F};
};

const Services* gServices = nullptr;
std::mutex gMutex; // guards everything below
std::vector<core::AlarmEntry> gTable;
std::optional<Ramp> gRamp;
std::int64_t gSleepDeadlineUs = 0;  // 0 = no sleep timer
std::int64_t gAlarmOffDeadlineUs = 0; // TurnOn duration; 0 = none
std::time_t gLastCheckedMinute = 0;

[[nodiscard]] std::int64_t nowUs()
{
    return esp_timer_get_time();
}

[[nodiscard]] std::optional<std::tm> localNow()
{
    const std::time_t now = std::time(nullptr);
    if (now < kValidEpoch) {
        return std::nullopt;
    }
    std::tm local{};
    localtime_r(&now, &local);
    return local;
}

[[nodiscard]] core::LocalTime toLocalTime(const std::tm& tm)
{
    return core::LocalTime{
        .weekday = core::weekdayFromTm(tm.tm_wday),
        .hour = static_cast<std::uint8_t>(tm.tm_hour),
        .minute = static_cast<std::uint8_t>(tm.tm_min),
    };
}

// ---- NVS ------------------------------------------------------------------

void saveTable(const std::string& json)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(kTag, "nvs_open failed, table not saved");
        return;
    }
    if (nvs_set_str(handle, kNvsKey, json.c_str()) != ESP_OK
        || nvs_commit(handle) != ESP_OK) {
        ESP_LOGE(kTag, "table not saved");
    }
    nvs_close(handle);
}

[[nodiscard]] std::string loadTableJson()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return {};
    }
    std::size_t length = 0U;
    std::string json;
    if (nvs_get_str(handle, kNvsKey, nullptr, &length) == ESP_OK
        && length > 0U) {
        json.resize(length);
        if (nvs_get_str(handle, kNvsKey, json.data(), &length) != ESP_OK) {
            json.clear();
        }
        json.resize(std::strlen(json.c_str()));
    }
    nvs_close(handle);
    return json;
}

void loadTable()
{
    const std::string json = loadTableJson();
    if (json.empty()) {
        return;
    }
    auto parsed = core::parseAlarmTableJson(json);
    if (!parsed) {
        ESP_LOGW(kTag, "saved table unreadable, ignored");
        return;
    }
    const std::lock_guard lock(gMutex);
    gTable = std::move(*parsed);
    ESP_LOGI(kTag, "%u alarm rows loaded", static_cast<unsigned>(gTable.size()));
}

// ---- Actions --------------------------------------------------------------

void setMaster(float db)
{
    const auto gain = core::GainDb::tryFromDb(db);
    if (!gain || !gServices->audio.setMasterVolume(*gain, *gain, false)) {
        ESP_LOGW(kTag, "master %.1f dB not applied", static_cast<double>(db));
    }
}

void stopWebRadio()
{
    core::WebRadioConfig config = gServices->webRadio.config();
    if (config.enabled) {
        config.enabled = false;
        (void)gServices->webRadio.setConfig(config);
    }
}

void startSource(const core::AlarmEntry& entry)
{
    if (entry.source == core::AlarmSource::WebRadio) {
        core::WebRadioConfig config{.enabled = true, .url = entry.url};
        if (!gServices->webRadio.setConfig(config)) {
            ESP_LOGW(kTag, "web radio not started");
        }
        return;
    }
    stopWebRadio();
    if (!gServices->integration.recallPreset(entry.preset)) {
        ESP_LOGW(kTag, "preset %u not recalled", entry.preset);
    }
}

/** The radio plays through the BT speaker: bring the link back if it dropped. */
void wakeSpeaker()
{
    const auto status = gServices->bluetooth.refreshStatus();
    if (status && status->a2dpState != core::Bt1035A2dpState::Connected) {
        (void)gServices->bluetooth.reconnectSavedSpeaker(false);
    }
}

void turnOff()
{
    {
        const std::lock_guard lock(gMutex);
        gRamp.reset();
        gSleepDeadlineUs = 0;
        gAlarmOffDeadlineUs = 0;
    }
    stopWebRadio();
    (void)gServices->audio.setPowerState(core::PowerState::Standby);
    ESP_LOGI(kTag, "standby");
}

void turnOn(const core::AlarmEntry& entry)
{
    // recallPreset loads the preset's audio profile, so the gain comes after.
    startSource(entry);
    const float startDb =
        core::alarmRampDb(entry.volumeDb, 0U, entry.fadeInSeconds);
    setMaster(startDb);
    (void)gServices->audio.setPowerState(core::PowerState::On);
    wakeSpeaker();
    const std::int64_t now = nowUs();
    const std::lock_guard lock(gMutex);
    gRamp = Ramp{.startUs = now,
                 .seconds = entry.fadeInSeconds,
                 .targetDb = entry.volumeDb,
                 .lastDb = startDb};
    gAlarmOffDeadlineUs =
        entry.durationMinutes == 0U
            ? 0
            : now + static_cast<std::int64_t>(entry.durationMinutes) * 60
                        * kUsPerSecond;
    ESP_LOGI(kTag, "on, %.1f dB", static_cast<double>(entry.volumeDb));
}

// ---- Tick -----------------------------------------------------------------

[[nodiscard]] bool userMovedVolume(float lastDb)
{
    const float master = gServices->audio.currentProfile().masterLeft.value();
    return std::fabs(master - lastDb) > kUserMoveDb;
}

void stepRamp()
{
    std::optional<Ramp> ramp;
    {
        const std::lock_guard lock(gMutex);
        ramp = gRamp;
    }
    if (!ramp) {
        return;
    }
    const auto elapsed =
        static_cast<std::uint32_t>((nowUs() - ramp->startUs) / kUsPerSecond);
    const bool moved = userMovedVolume(ramp->lastDb);
    const bool done = elapsed >= ramp->seconds || moved;
    const float db = core::alarmRampDb(ramp->targetDb, elapsed, ramp->seconds);
    if (!moved) {
        setMaster(db);
    }
    const std::lock_guard lock(gMutex);
    if (done) {
        gRamp.reset();
    } else if (gRamp) {
        gRamp->lastDb = db;
    }
}

[[nodiscard]] bool deadlinePassed(std::int64_t deadlineUs, std::int64_t now)
{
    return deadlineUs != 0 && now >= deadlineUs;
}

void checkDeadlines()
{
    if (gServices->audio.powerState() == core::PowerState::Standby) {
        // Switched off by hand: nothing left to switch off.
        const std::lock_guard lock(gMutex);
        gSleepDeadlineUs = 0;
        gAlarmOffDeadlineUs = 0;
        return;
    }
    bool expired = false;
    {
        const std::lock_guard lock(gMutex);
        const std::int64_t now = nowUs();
        expired = deadlinePassed(gSleepDeadlineUs, now)
                  || deadlinePassed(gAlarmOffDeadlineUs, now);
    }
    if (expired) {
        turnOff();
    }
}

void checkSchedule()
{
    const std::time_t minute = std::time(nullptr) / 60;
    const auto local = localNow();
    if (!local || minute == gLastCheckedMinute) {
        return;
    }
    gLastCheckedMinute = minute;
    std::optional<core::AlarmEntry> due;
    {
        const std::lock_guard lock(gMutex);
        if (const auto index = core::dueAlarm(gTable, toLocalTime(*local))) {
            due = gTable[*index];
        }
    }
    if (!due) {
        return;
    }
    if (due->action == core::AlarmAction::TurnOn) {
        turnOn(*due);
    } else {
        turnOff();
    }
}

void alarmTask(void* /*arg*/)
{
    for (;;) {
        stepRamp();
        checkDeadlines();
        checkSchedule();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---- HTTP API -------------------------------------------------------------

[[nodiscard]] core::AlarmClockInfo clockInfo(
    const std::vector<core::AlarmEntry>& table)
{
    core::AlarmClockInfo info;
    const auto local = localNow();
    if (!local) {
        return info;
    }
    char text[20];
    std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M", &*local);
    info.timeValid = true;
    info.localTime = text;
    info.next = core::nextAlarm(table, toLocalTime(*local));
    return info;
}

std::string statusJson()
{
    std::vector<core::AlarmEntry> table;
    {
        const std::lock_guard lock(gMutex);
        table = gTable;
    }
    return core::serializeAlarmStatusJson(table, clockInfo(table));
}

std::optional<std::string> replaceTable(std::string_view body)
{
    auto parsed = core::parseAlarmTableJson(body);
    if (!parsed) {
        return std::nullopt;
    }
    saveTable(core::serializeAlarmTableJson(*parsed));
    {
        const std::lock_guard lock(gMutex);
        gTable = std::move(*parsed);
    }
    return statusJson();
}

std::string sleepJson()
{
    std::int64_t deadline = 0;
    {
        const std::lock_guard lock(gMutex);
        deadline = gSleepDeadlineUs;
    }
    const std::int64_t left = deadline == 0 ? 0 : deadline - nowUs();
    return core::serializeSleepJson(
        left <= 0 ? 0U
                  : static_cast<std::uint32_t>((left + kUsPerSecond - 1)
                                               / kUsPerSecond));
}

std::optional<std::string> setSleep(std::string_view body)
{
    const auto minutes = core::parseSleepMinutesJson(body);
    if (!minutes) {
        return std::nullopt;
    }
    {
        const std::lock_guard lock(gMutex);
        gSleepDeadlineUs =
            *minutes == 0U
                ? 0
                : nowUs() + static_cast<std::int64_t>(*minutes) * 60
                                * kUsPerSecond;
    }
    ESP_LOGI(kTag, "sleep timer %u min", static_cast<unsigned>(*minutes));
    return sleepJson();
}

const net::AlarmApi kAlarmApi{
    .statusJson = &statusJson,
    .replaceTable = &replaceTable,
    .sleepJson = &sleepJson,
    .setSleep = &setSleep,
};

void startSntp()
{
    setenv("TZ", kTimeZone, 1);
    tzset();
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&config) != ESP_OK) {
        ESP_LOGW(kTag, "SNTP init failed: alarms wait for a valid clock");
    }
}

} // namespace

bool start(const Services& services)
{
    gServices = &services;
    startSntp();
    loadTable();
    net::setAlarmApi(&kAlarmApi);
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(alarmTask, "alarm", 6144, nullptr, 3, &handle) != pdPASS) {
        return false;
    }
    system_diag::watchTask("alarm", handle);
    return true;
}

} // namespace alarm_clock
