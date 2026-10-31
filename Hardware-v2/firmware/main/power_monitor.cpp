/**
 * @file    power_monitor.cpp
 * @brief   Polls the MAX17048 fuel gauge and the BQ25896 charger.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * The BQ25896 is only read and keeps its defaults, so its I2C watchdog has
 * nothing to revert. Both chips share one open-drain alert line: its pulse
 * from the BQ25896 (about 256 us) is too short to poll, so a falling edge
 * wakes the task, and the MAX17048's latched ALRT is cleared or it would hold
 * the line low and hide every later pulse. Register meanings: MAX17048 data sheet
 * (VCELL 0x02: 78.125 uV/LSB; SOC 0x04: 1/256 %), BQ25896 data sheet SLUSBL8
 * (REG0B status, REG0C faults). Neither document is in the repo yet: check
 * these at bring-up.
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */

#include "power_monitor.hpp"

#include "board_pins.hpp"
#include "board_v2.hpp"
#include "system_diag.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

namespace power_monitor {

namespace {

constexpr char kTag[] = "power";
constexpr int kI2cHz = 100000;
constexpr int kI2cTimeoutMs = 50;
constexpr TickType_t kPollPeriod = pdMS_TO_TICKS(30000);

constexpr std::uint8_t kMaxVcell = 0x02U;
constexpr std::uint8_t kMaxSoc = 0x04U;
constexpr std::uint8_t kMaxConfig = 0x0CU;
constexpr std::uint8_t kMaxConfigAlrt = 0x20U; // low byte of CONFIG
constexpr std::uint8_t kBqStatus = 0x0BU;
constexpr std::uint8_t kBqFault = 0x0CU;

i2c_master_dev_handle_t gGauge = nullptr;
i2c_master_dev_handle_t gCharger = nullptr;
TaskHandle_t gTask = nullptr;

std::atomic<std::uint16_t> gMillivolts{0U};
std::atomic<std::uint8_t> gPercent{0U};
std::atomic<std::uint8_t> gChargerStatus{0U};
std::atomic<std::uint8_t> gChargerFault{0U};
std::atomic<bool> gValid{false};

[[nodiscard]] i2c_master_dev_handle_t addDevice(int addr)
{
    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = static_cast<std::uint16_t>(addr);
    cfg.scl_speed_hz = kI2cHz;
    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(board_v2::i2cBus(), &cfg, &dev) != ESP_OK) {
        ESP_LOGW(kTag, "I2C device 0x%02X not added", addr);
        return nullptr;
    }
    return dev;
}

[[nodiscard]] bool readBytes(i2c_master_dev_handle_t dev, std::uint8_t reg,
                             std::uint8_t* data, std::size_t size)
{
    return dev != nullptr
           && i2c_master_transmit_receive(dev, &reg, 1U, data, size,
                                          kI2cTimeoutMs) == ESP_OK;
}

[[nodiscard]] bool pollGauge()
{
    std::array<std::uint8_t, 2> vcell = {};
    std::array<std::uint8_t, 2> soc = {};
    if (!readBytes(gGauge, kMaxVcell, vcell.data(), vcell.size())
        || !readBytes(gGauge, kMaxSoc, soc.data(), soc.size())) {
        return false;
    }
    const std::uint32_t raw = (static_cast<std::uint32_t>(vcell[0]) << 8) | vcell[1];
    gMillivolts.store(static_cast<std::uint16_t>((raw * 78125U) / 1000000U));
    gPercent.store(soc[0]);
    return true;
}

[[nodiscard]] bool pollCharger()
{
    std::uint8_t status = 0U;
    std::uint8_t fault = 0U;
    if (!readBytes(gCharger, kBqStatus, &status, 1U)
        || !readBytes(gCharger, kBqFault, &fault, 1U)) {
        return false;
    }
    gChargerStatus.store(status);
    gChargerFault.store(fault);
    return true;
}

void pollOnce()
{
    const bool gauge = pollGauge();
    const bool charger = pollCharger();
    if (gauge && charger && !gValid.load()) {
        ESP_LOGI(kTag, "battery %u mV, %u%%, charger status 0x%02X",
                 gMillivolts.load(), gPercent.load(), gChargerStatus.load());
    }
    gValid.store(gauge && charger);
}

/** Read-modify-write: CONFIG also holds the alert threshold. */
void clearGaugeAlert()
{
    std::array<std::uint8_t, 2> config = {};
    if (!readBytes(gGauge, kMaxConfig, config.data(), config.size())
        || (config[1] & kMaxConfigAlrt) == 0U) {
        return;
    }
    const std::array<std::uint8_t, 3> out = {
        kMaxConfig, config[0],
        static_cast<std::uint8_t>(config[1] & ~kMaxConfigAlrt)};
    if (i2c_master_transmit(gGauge, out.data(), out.size(), kI2cTimeoutMs)
        != ESP_OK) {
        ESP_LOGW(kTag, "MAX17048 alert not cleared");
    }
}

void alertIsr(void* /*arg*/)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(gTask, &woken);
    portYIELD_FROM_ISR(woken);
}

/** Without the edge the task simply polls. */
void armAlert()
{
    if (board::pins::PowerAlert < 0) {
        return;
    }
    const auto pin = static_cast<gpio_num_t>(board::pins::PowerAlert);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_FLOATING); // R11 is on the board
    gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE);
    const esp_err_t service = gpio_install_isr_service(0);
    if ((service != ESP_OK && service != ESP_ERR_INVALID_STATE)
        || gpio_isr_handler_add(pin, alertIsr, nullptr) != ESP_OK) {
        ESP_LOGW(kTag, "alert interrupt unavailable: polling only");
    }
}

[[noreturn]] void powerTask(void* /*arg*/)
{
    armAlert();
    while (true) {
        // A line already low at boot gives no edge; this first pass clears it.
        clearGaugeAlert();
        pollOnce();
        (void)ulTaskNotifyTake(pdTRUE, kPollPeriod);
    }
}

} // namespace

bool start()
{
    gGauge = addDevice(board::pins::Max17048Addr);
    gCharger = addDevice(board::pins::Bq25896Addr);
    if (xTaskCreate(powerTask, "power", 3072, nullptr, 2, &gTask) != pdPASS) {
        return false;
    }
    system_diag::watchTask("power", gTask);
    return true;
}

std::string systemJsonWithPower()
{
    std::string json = system_diag::json();
    std::array<char, 96> power = {};
    std::snprintf(power.data(), power.size(),
                  R"(,"power":{"valid":%s,"mv":%u,"percent":%u,"bq_status":%u,"bq_fault":%u}})",
                  gValid.load() ? "true" : "false", gMillivolts.load(),
                  gPercent.load(), gChargerStatus.load(), gChargerFault.load());
    if (!json.empty() && json.back() == '}') {
        json.pop_back();
        json += power.data();
    }
    return json;
}

} // namespace power_monitor
