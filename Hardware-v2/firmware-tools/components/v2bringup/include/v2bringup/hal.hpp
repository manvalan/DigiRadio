/**
 * @file    hal.hpp
 * @brief   Bare register access to every v2 block, for bring-up tools.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * No driver logic on purpose: each call does one bus operation, so a faulty
 * block shows up as the operation that fails. Pins come from
 * Hardware-v2/firmware/main/board_pins.hpp.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */
#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace bu {

// ---- I2C addresses and the registers both tools use ---------------------
inline constexpr std::uint8_t kBqAddr = 0x6B;   // BQ25896
inline constexpr std::uint8_t kFgAddr = 0x36;   // MAX17048
inline constexpr std::uint8_t kEeAddr = 0x50;   // 24AA025E64

// ---- Power domain -------------------------------------------------------
/** ENABLE_ANALOG high with every 3V3_AUDIO pin floating, then SMODE low. */
bool analogOn();
/** Release every audio pin, then ENABLE_ANALOG low. */
void analogOff();
bool analogIsOn();

// ---- I2C (3V3_DIGITAL) --------------------------------------------------
bool i2cInit();
esp_err_t i2cProbe(std::uint8_t addr);
esp_err_t i2cRead(std::uint8_t addr, std::uint8_t reg, std::uint8_t* data,
                  std::size_t n);
esp_err_t i2cWrite(std::uint8_t addr, std::uint8_t reg,
                   const std::uint8_t* data, std::size_t n);
/** MAX17048 16 bit registers, MSB first. */
esp_err_t i2cRead16(std::uint8_t addr, std::uint8_t reg, std::uint16_t& v);
esp_err_t i2cWrite16(std::uint8_t addr, std::uint8_t reg, std::uint16_t v);
esp_err_t i2cRead8(std::uint8_t addr, std::uint8_t reg, std::uint8_t& v);
esp_err_t i2cWrite8(std::uint8_t addr, std::uint8_t reg, std::uint8_t v);

// ---- Control SPI shared by ADAU1452 and Si4684 (3V3_AUDIO) -------------
/** Bus plus both devices; chip selects are driven by hand. */
bool spiInit();
void spiDeinit();
/** Same bus at another clock, for the monitor. */
bool spiSetHz(int hz);
int spiHz();

// ---- ADAU1452 -----------------------------------------------------------
/** RESET# low 10 ms then high; SELFBOOT released (R57 pull-up) or held low. */
void adauReset(bool selfBoot);
/** Three SS low pulses: the slave port switches from I2C to SPI. */
void adauToggleCs3();
/** Bytes per address step: registers (>= 0xF000) 2, memory 4. */
std::size_t adauWordBytes(std::uint16_t addr);
esp_err_t adauRead(std::uint16_t addr, std::uint8_t* data, std::size_t n);
esp_err_t adauWrite(std::uint16_t addr, const std::uint8_t* data,
                    std::size_t n);
esp_err_t adauReadReg(std::uint16_t addr, std::uint16_t& v);
esp_err_t adauWriteReg(std::uint16_t addr, std::uint16_t v);

// ---- Si4684 -------------------------------------------------------------
/** RSTB# low 5 ms, high, 20 ms wait (as the v1 driver). */
void siReset();
/**
 * Writes cmd, then polls RD_REPLY until CTS. reply gets STATUS0 onward.
 * @return ESP_ERR_TIMEOUT without CTS, ESP_ERR_INVALID_SIZE above 60 bytes.
 */
esp_err_t siCommand(const std::uint8_t* cmd, std::size_t n,
                    std::uint8_t* reply, std::size_t replyLen,
                    int timeoutMs = 1000);
/** One RD_REPLY, no wait. */
esp_err_t siReadReply(std::uint8_t* reply, std::size_t replyLen);

// ---- FSC-BT1058 ---------------------------------------------------------
/**
 * RESET# and SYS_CTRL low for 2.5 s (a module already on powers down), then
 * RESET# high, SYS_CTRL still low for 200 ms, then SYS_CTRL high.
 * Needs analogOn() first.
 */
bool btPowerUp();
/** RESET# and SYS_CTRL low, UART released. */
void btPowerDown();
bool btUartInit(int baud = 115200);
int btRead(char* buf, std::size_t n, int timeoutMs);
void btWrite(const char* text);
void btFlush();
/** Sends text plus CR LF, collects until OK/ERROR or timeout. */
std::string btAt(const char* cmd, int timeoutMs = 1500);
/** Prints text with control characters escaped. */
std::string printable(const char* data, std::size_t n);

// ---- Frequency counter on any GPIO (PCNT) -------------------------------
bool measureEdges(int gpio, int windowMs, long& edges, double& hz);

// ---- PWR_ALERT (PMIC_INT + MAX17048_ALERT) -------------------------------
bool alertArm();
int alertEdges();
void alertClearCount();
int alertLevel();

// ---- U3 W25Q128 on its own SPI bus ---------------------------------------
esp_err_t extFlashJedec(std::uint8_t id[3]);

// ---- I2S to the ADAU1452 (ESP32 slave) ----------------------------------
/** 1 kHz-ish sine on DOUT for the given time, clocks from the DSP. */
esp_err_t i2sTone(int hz, int seconds, int sampleRate = 48000);
/** Reads DIN for windowMs; peak absolute sample of each channel. */
esp_err_t i2sPeak(int windowMs, long& peakL, long& peakR, long& frames);

} // namespace bu
