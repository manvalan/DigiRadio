/**
 * @file    Bt1035Driver.cpp
 * @brief   Bt1035Driver implementation.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "bt1035/Bt1035Driver.hpp"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <string>
#include <string_view>

namespace bt1035 {

namespace {
constexpr char kTag[] = "Bt1035";
constexpr int kUartPort = 2;
constexpr int kBaudRate = 115200;
constexpr int kUartRxBuffer = 512;
constexpr int kUartTxBuffer = 256;
constexpr int kResponseTimeoutMs = 2000;
constexpr int kPostResetMs = 300;
constexpr int kPostUartMs = 100;
} // namespace

Bt1035Driver::Bt1035Driver(Bt1035Pins pins)
    : pins_(pins)
    , booted_(false)
    , uartInstalled_(false)
    , uartPort_(kUartPort)
{
}

Bt1035Driver::~Bt1035Driver()
{
    if (uartInstalled_) {
        uart_driver_delete(static_cast<uart_port_t>(uartPort_));
        uartInstalled_ = false;
    }
}

bool Bt1035Driver::isBooted() const noexcept
{
    return booted_;
}

std::expected<void, Bt1035Error> Bt1035Driver::ensureBooted() const
{
    if (!booted_) {
        return std::unexpected(Bt1035Error::NotBooted);
    }
    return {};
}

std::expected<std::string, Bt1035Error> Bt1035Driver::transmitAndCollect(
    std::string_view commandLine)
{
    const int written = uart_write_bytes(static_cast<uart_port_t>(uartPort_),
                                         commandLine.data(),
                                         commandLine.size());
    if (written < 0
        || static_cast<std::size_t>(written) != commandLine.size()) {
        return std::unexpected(Bt1035Error::UartInitFailed);
    }

    std::array<char, 128> buffer{};
    std::string accumulated;
    const TickType_t deadline =
        xTaskGetTickCount() + pdMS_TO_TICKS(kResponseTimeoutMs);

    while (xTaskGetTickCount() < deadline) {
        const int received = uart_read_bytes(static_cast<uart_port_t>(uartPort_),
                                             buffer.data(), buffer.size(),
                                             pdMS_TO_TICKS(50));
        if (received > 0) {
            accumulated.append(buffer.data(), static_cast<std::size_t>(received));
            const core::Bt1035AtResponseKind kind =
                core::parseBt1035AtResponse(accumulated);
            if (kind == core::Bt1035AtResponseKind::Ok) {
                return accumulated;
            }
            if (kind == core::Bt1035AtResponseKind::Error) {
                return std::unexpected(Bt1035Error::AtError);
            }
        }
    }

    return std::unexpected(Bt1035Error::AtTimeout);
}

std::expected<void, Bt1035Error> Bt1035Driver::transmitAndExpectOk(
    std::string_view commandLine)
{
    if (auto collected = transmitAndCollect(commandLine); collected) {
        return {};
    }
    return std::unexpected(collected.error());
}

std::expected<void, Bt1035Error> Bt1035Driver::sendCommand(
    core::Bt1035AtCommand command)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return transmitAndExpectOk(core::buildBt1035AtLine(command));
}

std::expected<void, Bt1035Error> Bt1035Driver::enterPairingMode()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::PairDiscoverable);
}

std::expected<void, Bt1035Error> Bt1035Driver::leavePairingMode()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::PairHidden);
}

std::expected<core::Bt1035A2dpState, Bt1035Error> Bt1035Driver::queryA2dpState()
{
    if (auto ready = ensureBooted(); !ready) {
        return std::unexpected(ready.error());
    }

    auto response =
        transmitAndCollect(core::buildBt1035AtLine(core::Bt1035AtCommand::A2dpStat));
    if (!response) {
        return std::unexpected(response.error());
    }

    auto parsed = core::parseBt1035A2dpStatResponse(*response);
    if (!parsed) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    return *parsed;
}

std::expected<void, Bt1035Error> Bt1035Driver::disconnectA2dp()
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    return sendCommand(core::Bt1035AtCommand::A2dpDisconnect);
}

std::expected<void, Bt1035Error> Bt1035Driver::setDeviceName(
    std::string_view name)
{
    if (auto ready = ensureBooted(); !ready) {
        return ready;
    }
    if (name.empty() || name.size() > 32U) {
        return std::unexpected(Bt1035Error::UnexpectedResponse);
    }
    std::string line = std::string("AT+NAME=") + std::string(name) + "\r\n";
    return transmitAndExpectOk(line);
}

std::expected<void, Bt1035Error> Bt1035Driver::runInitSequence()
{
    for (const core::Bt1035AtCommand command : core::bootInitSequence()) {
        if (auto result = transmitAndExpectOk(core::buildBt1035AtLine(command));
            !result) {
            return result;
        }
    }
    return {};
}

std::expected<void, Bt1035Error> Bt1035Driver::boot()
{
    if (booted_) {
        return {};
    }

    gpio_config_t resetCfg = {};
    resetCfg.pin_bit_mask = 1ULL << pins_.resetGpio;
    resetCfg.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&resetCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    gpio_config_t sysCfg = {};
    sysCfg.pin_bit_mask = 1ULL << pins_.sysCtlGpio;
    sysCfg.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&sysCfg) != ESP_OK) {
        return std::unexpected(Bt1035Error::ResetFailed);
    }

    gpio_set_level(static_cast<gpio_num_t>(pins_.sysCtlGpio), 1);
    gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(static_cast<gpio_num_t>(pins_.resetGpio), 1);
    vTaskDelay(pdMS_TO_TICKS(kPostResetMs));

    if (!uartInstalled_) {
        const uart_config_t uartCfg = {
            .baud_rate = kBaudRate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
            .rx_flow_ctrl_thresh = 122,
            .source_clk = UART_SCLK_DEFAULT,
        };

        if (uart_driver_install(static_cast<uart_port_t>(uartPort_), kUartRxBuffer,
                              kUartTxBuffer, 0, nullptr, 0)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        if (uart_param_config(static_cast<uart_port_t>(uartPort_), &uartCfg)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        if (uart_set_pin(static_cast<uart_port_t>(uartPort_), pins_.uartTx,
                         pins_.uartRx, pins_.rtsGpio, pins_.ctsGpio)
            != ESP_OK) {
            return std::unexpected(Bt1035Error::UartInitFailed);
        }

        uartInstalled_ = true;
    }

    uart_flush_input(static_cast<uart_port_t>(uartPort_));
    vTaskDelay(pdMS_TO_TICKS(kPostUartMs));

    if (auto init = runInitSequence(); !init) {
        ESP_LOGE(kTag, "AT init failed");
        return init;
    }

    booted_ = true;
    ESP_LOGI(kTag, "Line-In mode enabled (AT+AUXCFG=1)");
    return {};
}

} // namespace bt1035
