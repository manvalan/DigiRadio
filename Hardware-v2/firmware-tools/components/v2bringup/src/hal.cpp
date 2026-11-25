/**
 * @file    hal.cpp
 * @brief   Bare register access to every v2 block.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Sources: ADAU1452 data sheet Rev. D (SPI slave port, Table 27); AN649
 * (Si4684 RD_REPLY/CTS); BT1035/BT1058 driver of the v1 firmware for the
 * SYS_CTRL/RESET# sequence.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/hal.hpp"

#include "board_pins.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "driver/pulse_cnt.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace bu {

namespace {

namespace pins = board::pins;

constexpr int kI2cHz = 100000;
constexpr int kI2cTimeoutMs = 50;
constexpr int kRailSettleMs = 20;
/** Without DMA a transaction holds at most 64 bytes. */
constexpr std::size_t kSpiMax = 64U;
constexpr int kBtUart = 1;

constexpr std::array<int, 17> kAudioPins = {
    pins::SpiSclk,    pins::SpiMosi,    pins::SpiMiso,  pins::Si4684Cs,
    pins::Si4684Rstb, pins::Si4684Intb, pins::Si4684Smode, pins::AdauCs,
    pins::AdauReset,  pins::I2sBclk,    pins::I2sWs,    pins::I2sDout,
    pins::I2sDin,     pins::BtUartRx,   pins::BtUartTx, pins::BtSysCtrl,
    pins::AdauSelfBoot,
};

bool gAnalogOn = false;
i2c_master_bus_handle_t gI2c = nullptr;
std::array<i2c_master_dev_handle_t, 128> gI2cDev = {};
bool gSpiBus = false;
spi_device_handle_t gAdau = nullptr;
spi_device_handle_t gSi = nullptr;
int gSpiHz = 1000000;
bool gUart = false;
std::atomic<int> gAlertEdges{0};
bool gAlertArmed = false;

gpio_num_t g(int pin)
{
    return static_cast<gpio_num_t>(pin);
}

void floatPin(int pin)
{
    gpio_reset_pin(g(pin));
    gpio_set_direction(g(pin), GPIO_MODE_DISABLE);
    gpio_set_pull_mode(g(pin), GPIO_FLOATING);
}

void outPin(int pin, int level)
{
    gpio_set_level(g(pin), level);
    gpio_set_direction(g(pin), GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(g(pin), level);
}

i2c_master_dev_handle_t dev(std::uint8_t addr)
{
    if (gI2c == nullptr || addr > 0x7F) {
        return nullptr;
    }
    if (gI2cDev[addr] == nullptr) {
        i2c_device_config_t cfg = {};
        cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        cfg.device_address = addr;
        cfg.scl_speed_hz = kI2cHz;
        if (i2c_master_bus_add_device(gI2c, &cfg, &gI2cDev[addr]) != ESP_OK) {
            gI2cDev[addr] = nullptr;
        }
    }
    return gI2cDev[addr];
}

spi_device_handle_t addSpi(int mode)
{
    spi_device_interface_config_t cfg = {};
    cfg.clock_speed_hz = gSpiHz;
    cfg.mode = static_cast<std::uint8_t>(mode);
    cfg.spics_io_num = -1; // chip selects by hand, see csXfer()
    cfg.queue_size = 1;
    spi_device_handle_t h = nullptr;
    if (spi_bus_add_device(SPI2_HOST, &cfg, &h) != ESP_OK) {
        return nullptr;
    }
    return h;
}

esp_err_t csXfer(spi_device_handle_t h, int cs, const std::uint8_t* tx,
                 std::uint8_t* rx, std::size_t n)
{
    if (h == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (n == 0U || n > kSpiMax) {
        return ESP_ERR_INVALID_SIZE;
    }
    spi_transaction_t t = {};
    t.length = n * 8U;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    gpio_set_level(g(cs), 0);
    esp_rom_delay_us(1);
    const esp_err_t err = spi_device_polling_transmit(h, &t);
    esp_rom_delay_us(1);
    gpio_set_level(g(cs), 1);
    esp_rom_delay_us(1);
    return err;
}

void IRAM_ATTR alertIsr(void* /*arg*/)
{
    gAlertEdges.fetch_add(1);
}

} // namespace

// ---- Power domain -------------------------------------------------------

bool analogOn()
{
    for (const int pin : kAudioPins) {
        floatPin(pin);
    }
    floatPin(pins::BtReset);
    gpio_set_level(g(pins::EnableAnalog), 1);
    if (gpio_set_direction(g(pins::EnableAnalog), GPIO_MODE_INPUT_OUTPUT)
        != ESP_OK) {
        return false;
    }
    gpio_set_level(g(pins::EnableAnalog), 1);
    vTaskDelay(pdMS_TO_TICKS(kRailSettleMs));
    outPin(pins::Si4684Smode, 0); // SPI mode for the Si4684
    gAnalogOn = true;
    return true;
}

void analogOff()
{
    btPowerDown();
    spiDeinit();
    for (const int pin : kAudioPins) {
        floatPin(pin);
    }
    floatPin(pins::BtReset);
    gpio_set_level(g(pins::EnableAnalog), 0);
    gpio_set_direction(g(pins::EnableAnalog), GPIO_MODE_INPUT_OUTPUT);
    gpio_set_level(g(pins::EnableAnalog), 0);
    gAnalogOn = false;
}

bool analogIsOn()
{
    return gAnalogOn;
}

// ---- I2C ----------------------------------------------------------------

bool i2cInit()
{
    if (gI2c != nullptr) {
        return true;
    }
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = 0;
    cfg.sda_io_num = g(pins::I2cSda);
    cfg.scl_io_num = g(pins::I2cScl);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = false; // the board has its own
    return i2c_new_master_bus(&cfg, &gI2c) == ESP_OK;
}

esp_err_t i2cProbe(std::uint8_t addr)
{
    if (gI2c == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_probe(gI2c, addr, kI2cTimeoutMs);
}

esp_err_t i2cRead(std::uint8_t addr, std::uint8_t reg, std::uint8_t* data,
                  std::size_t n)
{
    auto* d = dev(addr);
    if (d == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(d, &reg, 1U, data, n, kI2cTimeoutMs);
}

esp_err_t i2cWrite(std::uint8_t addr, std::uint8_t reg,
                   const std::uint8_t* data, std::size_t n)
{
    auto* d = dev(addr);
    if (d == nullptr || n > 32U) {
        return ESP_ERR_INVALID_ARG;
    }
    std::array<std::uint8_t, 33> buf = {};
    buf[0] = reg;
    std::memcpy(buf.data() + 1, data, n);
    return i2c_master_transmit(d, buf.data(), n + 1U, kI2cTimeoutMs);
}

esp_err_t i2cRead16(std::uint8_t addr, std::uint8_t reg, std::uint16_t& v)
{
    std::array<std::uint8_t, 2> b = {};
    const esp_err_t err = i2cRead(addr, reg, b.data(), 2U);
    v = static_cast<std::uint16_t>((b[0] << 8) | b[1]);
    return err;
}

esp_err_t i2cWrite16(std::uint8_t addr, std::uint8_t reg, std::uint16_t v)
{
    const std::array<std::uint8_t, 2> b = {
        static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)};
    return i2cWrite(addr, reg, b.data(), 2U);
}

esp_err_t i2cRead8(std::uint8_t addr, std::uint8_t reg, std::uint8_t& v)
{
    return i2cRead(addr, reg, &v, 1U);
}

esp_err_t i2cWrite8(std::uint8_t addr, std::uint8_t reg, std::uint8_t v)
{
    return i2cWrite(addr, reg, &v, 1U);
}

// ---- SPI ----------------------------------------------------------------

bool spiInit()
{
    if (!gSpiBus) {
        spi_bus_config_t cfg = {};
        cfg.miso_io_num = pins::SpiMiso;
        cfg.mosi_io_num = pins::SpiMosi;
        cfg.sclk_io_num = pins::SpiSclk;
        cfg.quadwp_io_num = -1;
        cfg.quadhd_io_num = -1;
        cfg.max_transfer_sz = static_cast<int>(kSpiMax);
        if (spi_bus_initialize(SPI2_HOST, &cfg, SPI_DMA_DISABLED) != ESP_OK) {
            return false;
        }
        gSpiBus = true;
    }
    outPin(pins::AdauCs, 1);
    outPin(pins::Si4684Cs, 1);
    if (gAdau == nullptr) {
        gAdau = addSpi(3); // ADAU1452: CPOL = CPHA = 1
    }
    if (gSi == nullptr) {
        gSi = addSpi(0); // Si4684: mode 0
    }
    return gAdau != nullptr && gSi != nullptr;
}

void spiDeinit()
{
    if (gAdau != nullptr) {
        spi_bus_remove_device(gAdau);
        gAdau = nullptr;
    }
    if (gSi != nullptr) {
        spi_bus_remove_device(gSi);
        gSi = nullptr;
    }
    if (gSpiBus) {
        spi_bus_free(SPI2_HOST);
        gSpiBus = false;
    }
}

bool spiSetHz(int hz)
{
    gSpiHz = hz;
    if (!gSpiBus) {
        return true;
    }
    spiDeinit();
    return spiInit();
}

int spiHz()
{
    return gSpiHz;
}

// ---- ADAU1452 -----------------------------------------------------------

void adauReset(bool selfBoot)
{
    outPin(pins::AdauReset, 0);
    if (selfBoot) {
        floatPin(pins::AdauSelfBoot); // R57 pulls it high
    } else {
        outPin(pins::AdauSelfBoot, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(g(pins::AdauReset), 1);
    // Crystal start-up and PLL lock (Table 27: at most 10.666 ms).
    vTaskDelay(pdMS_TO_TICKS(20));
}

void adauToggleCs3()
{
    // Each level must last at least one MCLK cycle (81 ns at 12.288 MHz).
    outPin(pins::AdauCs, 1);
    for (int i = 0; i < 3; ++i) {
        gpio_set_level(g(pins::AdauCs), 0);
        esp_rom_delay_us(2);
        gpio_set_level(g(pins::AdauCs), 1);
        esp_rom_delay_us(2);
    }
}

std::size_t adauWordBytes(std::uint16_t addr)
{
    return addr >= 0xF000U ? 2U : 4U;
}

esp_err_t adauRead(std::uint16_t addr, std::uint8_t* data, std::size_t n)
{
    const std::size_t step = adauWordBytes(addr);
    std::size_t done = 0U;
    while (done < n) {
        const std::size_t chunk = std::min<std::size_t>(60U, n - done);
        std::array<std::uint8_t, kSpiMax> tx = {};
        std::array<std::uint8_t, kSpiMax> rx = {};
        const auto at = static_cast<std::uint16_t>(addr + done / step);
        tx[0] = 0x01; // chip address 0, read
        tx[1] = static_cast<std::uint8_t>(at >> 8);
        tx[2] = static_cast<std::uint8_t>(at);
        const esp_err_t err =
            csXfer(gAdau, pins::AdauCs, tx.data(), rx.data(), 3U + chunk);
        if (err != ESP_OK) {
            return err;
        }
        std::memcpy(data + done, rx.data() + 3, chunk);
        done += chunk;
    }
    return ESP_OK;
}

esp_err_t adauWrite(std::uint16_t addr, const std::uint8_t* data,
                    std::size_t n)
{
    const std::size_t step = adauWordBytes(addr);
    std::size_t done = 0U;
    while (done < n) {
        const std::size_t chunk = std::min<std::size_t>(60U, n - done);
        std::array<std::uint8_t, kSpiMax> tx = {};
        const auto at = static_cast<std::uint16_t>(addr + done / step);
        tx[0] = 0x00; // chip address 0, write
        tx[1] = static_cast<std::uint8_t>(at >> 8);
        tx[2] = static_cast<std::uint8_t>(at);
        std::memcpy(tx.data() + 3, data + done, chunk);
        const esp_err_t err =
            csXfer(gAdau, pins::AdauCs, tx.data(), nullptr, 3U + chunk);
        if (err != ESP_OK) {
            return err;
        }
        done += chunk;
    }
    return ESP_OK;
}

esp_err_t adauReadReg(std::uint16_t addr, std::uint16_t& v)
{
    std::array<std::uint8_t, 2> b = {};
    const esp_err_t err = adauRead(addr, b.data(), 2U);
    v = static_cast<std::uint16_t>((b[0] << 8) | b[1]);
    return err;
}

esp_err_t adauWriteReg(std::uint16_t addr, std::uint16_t v)
{
    const std::array<std::uint8_t, 2> b = {
        static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)};
    return adauWrite(addr, b.data(), 2U);
}

// ---- Si4684 -------------------------------------------------------------

void siReset()
{
    outPin(pins::Si4684Rstb, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(g(pins::Si4684Rstb), 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_reset_pin(g(pins::Si4684Intb));
    gpio_set_direction(g(pins::Si4684Intb), GPIO_MODE_INPUT);
    gpio_set_pull_mode(g(pins::Si4684Intb), GPIO_PULLUP_ONLY);
}

esp_err_t siReadReply(std::uint8_t* reply, std::size_t replyLen)
{
    if (replyLen + 1U > kSpiMax) {
        return ESP_ERR_INVALID_SIZE;
    }
    // AN649: RD_REPLY (0x00) then the reply; byte 0 of rx is the lead-in.
    std::array<std::uint8_t, kSpiMax> tx = {};
    std::array<std::uint8_t, kSpiMax> rx = {};
    const esp_err_t err =
        csXfer(gSi, pins::Si4684Cs, tx.data(), rx.data(), replyLen + 1U);
    std::memcpy(reply, rx.data() + 1, replyLen);
    return err;
}

esp_err_t siCommand(const std::uint8_t* cmd, std::size_t n,
                    std::uint8_t* reply, std::size_t replyLen, int timeoutMs)
{
    if (replyLen < 4U || replyLen + 1U > kSpiMax || n > kSpiMax) {
        return ESP_ERR_INVALID_SIZE;
    }
    std::array<std::uint8_t, kSpiMax> tx = {};
    std::memcpy(tx.data(), cmd, n);
    if (const esp_err_t err =
            csXfer(gSi, pins::Si4684Cs, tx.data(), nullptr, n);
        err != ESP_OK) {
        return err;
    }
    const int64_t end = esp_timer_get_time() + timeoutMs * 1000LL;
    do {
        esp_rom_delay_us(500);
        if (const esp_err_t err = siReadReply(reply, replyLen); err != ESP_OK) {
            return err;
        }
        if ((reply[0] & 0x80U) != 0U) {
            return ESP_OK;
        }
    } while (esp_timer_get_time() < end);
    return ESP_ERR_TIMEOUT;
}

// ---- BT1058 -------------------------------------------------------------

bool btPowerUp()
{
    outPin(pins::BtReset, 0);
    outPin(pins::BtSysCtrl, 0);
    vTaskDelay(pdMS_TO_TICKS(2500)); // past the ~1.8 s reset protection
    gpio_set_level(g(pins::BtReset), 1);
    vTaskDelay(pdMS_TO_TICKS(200)); // SYS_CTRL low >= 100 ms
    gpio_set_level(g(pins::BtSysCtrl), 1);
    return btUartInit();
}

void btPowerDown()
{
    if (gUart) {
        uart_driver_delete(static_cast<uart_port_t>(kBtUart));
        gUart = false;
    }
    if (gAnalogOn) {
        outPin(pins::BtReset, 0);
        outPin(pins::BtSysCtrl, 0);
    }
}

bool btUartInit(int baud)
{
    if (gUart) {
        return uart_set_baudrate(static_cast<uart_port_t>(kBtUart),
                                 static_cast<uint32_t>(baud)) == ESP_OK;
    }
    uart_config_t cfg = {};
    cfg.baud_rate = baud;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    const auto port = static_cast<uart_port_t>(kBtUart);
    if (uart_driver_install(port, 4096, 0, 0, nullptr, 0) != ESP_OK) {
        return false;
    }
    gUart = uart_param_config(port, &cfg) == ESP_OK
            && uart_set_pin(port, pins::BtUartTx, pins::BtUartRx,
                            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)
                   == ESP_OK;
    if (!gUart) {
        uart_driver_delete(port);
    }
    return gUart;
}

int btRead(char* buf, std::size_t n, int timeoutMs)
{
    if (!gUart) {
        return -1;
    }
    return uart_read_bytes(static_cast<uart_port_t>(kBtUart), buf,
                           static_cast<uint32_t>(n), pdMS_TO_TICKS(timeoutMs));
}

void btWrite(const char* text)
{
    if (gUart) {
        uart_write_bytes(static_cast<uart_port_t>(kBtUart), text,
                         std::strlen(text));
    }
}

void btFlush()
{
    if (gUart) {
        uart_flush_input(static_cast<uart_port_t>(kBtUart));
    }
}

std::string btAt(const char* cmd, int timeoutMs)
{
    btFlush();
    btWrite(cmd);
    btWrite("\r\n");
    std::string out;
    const int64_t end = esp_timer_get_time() + timeoutMs * 1000LL;
    std::array<char, 128> buf = {};
    while (esp_timer_get_time() < end) {
        const int n = btRead(buf.data(), buf.size(), 50);
        if (n > 0) {
            out.append(buf.data(), static_cast<std::size_t>(n));
            if (out.find("OK\r\n") != std::string::npos
                || out.find("ERR") != std::string::npos) {
                // a short grace for trailing lines
                const int m = btRead(buf.data(), buf.size(), 100);
                if (m > 0) {
                    out.append(buf.data(), static_cast<std::size_t>(m));
                }
                break;
            }
        }
    }
    return out;
}

std::string printable(const char* data, std::size_t n)
{
    std::string s;
    for (std::size_t i = 0; i < n; ++i) {
        const auto c = static_cast<unsigned char>(data[i]);
        if (c == '\r') {
            s += "\\r";
        } else if (c == '\n') {
            s += "\\n";
        } else if (c >= 0x20 && c < 0x7F) {
            s += static_cast<char>(c);
        } else {
            char hex[8];
            std::snprintf(hex, sizeof hex, "\\x%02X", c);
            s += hex;
        }
    }
    return s;
}

// ---- PCNT ---------------------------------------------------------------

bool measureEdges(int gpio, int windowMs, long& edges, double& hz)
{
    edges = 0;
    hz = 0.0;
    constexpr int kLimit = 30000;
    pcnt_unit_config_t ucfg = {};
    ucfg.low_limit = -1;
    ucfg.high_limit = kLimit;
    ucfg.flags.accum_count = 1;
    pcnt_unit_handle_t unit = nullptr;
    if (pcnt_new_unit(&ucfg, &unit) != ESP_OK) {
        return false;
    }
    pcnt_chan_config_t ccfg = {};
    ccfg.edge_gpio_num = gpio;
    ccfg.level_gpio_num = -1;
    pcnt_channel_handle_t chan = nullptr;
    bool ok = pcnt_new_channel(unit, &ccfg, &chan) == ESP_OK;
    ok = ok
         && pcnt_channel_set_edge_action(chan,
                                         PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                         PCNT_CHANNEL_EDGE_ACTION_HOLD)
                == ESP_OK
         && pcnt_unit_add_watch_point(unit, kLimit) == ESP_OK
         && pcnt_unit_enable(unit) == ESP_OK
         && pcnt_unit_clear_count(unit) == ESP_OK
         && pcnt_unit_start(unit) == ESP_OK;
    if (ok) {
        const int64_t t0 = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(windowMs));
        int count = 0;
        pcnt_unit_get_count(unit, &count);
        const int64_t t1 = esp_timer_get_time();
        pcnt_unit_stop(unit);
        pcnt_unit_disable(unit);
        edges = count;
        hz = (t1 > t0) ? count * 1e6 / static_cast<double>(t1 - t0) : 0.0;
    }
    if (chan != nullptr) {
        pcnt_del_channel(chan);
    }
    pcnt_del_unit(unit);
    floatPin(gpio);
    return ok;
}

// ---- PWR_ALERT ----------------------------------------------------------

bool alertArm()
{
    if (gAlertArmed) {
        return true;
    }
    const auto pin = g(pins::PowerAlert);
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_FLOATING); // R11 on the board
    gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE);
    const esp_err_t svc = gpio_install_isr_service(0);
    if (svc != ESP_OK && svc != ESP_ERR_INVALID_STATE) {
        return false;
    }
    gAlertArmed = gpio_isr_handler_add(pin, alertIsr, nullptr) == ESP_OK;
    return gAlertArmed;
}

int alertEdges()
{
    return gAlertEdges.load();
}

void alertClearCount()
{
    gAlertEdges.store(0);
}

int alertLevel()
{
    return gpio_get_level(g(pins::PowerAlert));
}

// ---- U3 W25Q128 ---------------------------------------------------------

esp_err_t extFlashJedec(std::uint8_t id[3])
{
    spi_bus_config_t bus = {};
    bus.miso_io_num = pins::ExtFlashMiso;
    bus.mosi_io_num = pins::ExtFlashMosi;
    bus.sclk_io_num = pins::ExtFlashSclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 16;
    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED);
    if (err != ESP_OK) {
        return err;
    }
    spi_device_interface_config_t cfg = {};
    cfg.clock_speed_hz = 1000000;
    cfg.mode = 0;
    cfg.spics_io_num = pins::ExtFlashCs;
    cfg.queue_size = 1;
    spi_device_handle_t h = nullptr;
    err = spi_bus_add_device(SPI3_HOST, &cfg, &h);
    if (err == ESP_OK) {
        std::array<std::uint8_t, 4> tx = {0x9F, 0, 0, 0}; // JEDEC ID
        std::array<std::uint8_t, 4> rx = {};
        spi_transaction_t t = {};
        t.length = 32;
        t.tx_buffer = tx.data();
        t.rx_buffer = rx.data();
        err = spi_device_polling_transmit(h, &t);
        std::memcpy(id, rx.data() + 1, 3);
        spi_bus_remove_device(h);
    }
    spi_bus_free(SPI3_HOST);
    return err;
}

// ---- I2S ----------------------------------------------------------------

namespace {

i2s_chan_handle_t gTx = nullptr;
i2s_chan_handle_t gRx = nullptr;

esp_err_t i2sOpen(int rate)
{
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                      I2S_ROLE_SLAVE);
    if (esp_err_t err = i2s_new_channel(&ch, &gTx, &gRx); err != ESP_OK) {
        return err;
    }
    i2s_std_config_t cfg = {};
    cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(rate));
    // The DSP runs 64 BCLK per frame: 32 bit slots.
    cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                       I2S_SLOT_MODE_STEREO);
    cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.bclk = g(pins::I2sBclk);
    cfg.gpio_cfg.ws = g(pins::I2sWs);
    cfg.gpio_cfg.dout = g(pins::I2sDout);
    cfg.gpio_cfg.din = g(pins::I2sDin);
    esp_err_t err = i2s_channel_init_std_mode(gTx, &cfg);
    if (err == ESP_OK) {
        err = i2s_channel_init_std_mode(gRx, &cfg);
    }
    return err;
}

void i2sClose()
{
    if (gTx != nullptr) {
        i2s_channel_disable(gTx);
        i2s_del_channel(gTx);
        gTx = nullptr;
    }
    if (gRx != nullptr) {
        i2s_channel_disable(gRx);
        i2s_del_channel(gRx);
        gRx = nullptr;
    }
    for (const int pin : {pins::I2sBclk, pins::I2sWs, pins::I2sDout,
                          pins::I2sDin}) {
        floatPin(pin);
    }
}

} // namespace

esp_err_t i2sTone(int hz, int seconds, int sampleRate)
{
    esp_err_t err = i2sOpen(sampleRate);
    if (err == ESP_OK) {
        err = i2s_channel_enable(gTx);
    }
    std::array<std::int32_t, 256> buf = {};
    double phase = 0.0;
    const double step = 2.0 * M_PI * hz / sampleRate;
    const int64_t end = esp_timer_get_time() + seconds * 1000000LL;
    while (err == ESP_OK && esp_timer_get_time() < end) {
        for (std::size_t i = 0; i < buf.size(); i += 2) {
            // -12 dBFS
            const auto s = static_cast<std::int32_t>(
                std::sin(phase) * 0.25 * 2147483647.0);
            buf[i] = s;
            buf[i + 1] = s;
            phase += step;
            if (phase > 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
        size_t written = 0;
        err = i2s_channel_write(gTx, buf.data(), sizeof buf, &written, 1000);
    }
    i2sClose();
    return err;
}

esp_err_t i2sPeak(int windowMs, long& peakL, long& peakR, long& frames)
{
    peakL = peakR = frames = 0;
    esp_err_t err = i2sOpen(48000);
    if (err == ESP_OK) {
        err = i2s_channel_enable(gRx);
    }
    std::array<std::int32_t, 256> buf = {};
    const int64_t end = esp_timer_get_time() + windowMs * 1000LL;
    while (err == ESP_OK && esp_timer_get_time() < end) {
        size_t got = 0;
        err = i2s_channel_read(gRx, buf.data(), sizeof buf, &got, 500);
        for (std::size_t i = 0; i + 1 < got / 4U; i += 2) {
            peakL = std::max(peakL, std::labs(buf[i] >> 8));
            peakR = std::max(peakR, std::labs(buf[i + 1] >> 8));
            ++frames;
        }
    }
    i2sClose();
    return err;
}

} // namespace bu
