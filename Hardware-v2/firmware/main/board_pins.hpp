/**
 * @file    board_pins.hpp
 * @brief   DigiRadio v2 ESP32-S31 board pin map — single source of truth.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * From the netlist of Hardware-v2/kicad/digiradio-batch2.kicad_sch. Rails:
 * 3V3_DIGITAL is always on; 3V3_AUDIO (Si4684 VIO, ADAU1452, BT1058) follows
 * ENABLE_ANALOG, so every pin to that domain stays high impedance while it
 * is off (see board_v2.hpp).
 *
 * @author  Michele Bigi
 * @date    2026-10-31
 */
#pragma once

namespace board::pins {

// ---- Power ----------------------------------------------------------
inline constexpr int EnableAnalog = 19; // U11 EN, 10k pull-down

// ---- Control SPI, shared by Si4684 and ADAU1452 (3V3_AUDIO) ----------
inline constexpr int SpiSclk = 1; // via R49
inline constexpr int SpiMosi = 2; // via R45
inline constexpr int SpiMiso = 3; // via R48

// ---- Si4684 tuner ---------------------------------------------------
inline constexpr int Si4684Cs    = 21; // SSB (pin 29), pull-up
inline constexpr int Si4684Rstb  = 15; // RSTB# (pin 4)
inline constexpr int Si4684Intb  = 16; // INTB# (pin 3)
inline constexpr int Si4684Smode = 17; // SMODE (pin 5): low selects SPI

// ---- ADAU1452 DSP ---------------------------------------------------
inline constexpr int AdauCs    = 20; // SS/ADDR0 (pin 33), pull-up
inline constexpr int AdauReset = 14; // RESET# (pin 24), 10k pull-down
inline constexpr int AdauSelfBoot = 22; // SELFBOOT (pin 34), 10k pull-up (R57)

// ---- I2C: 24AA025E64, MAX17048, BQ25896 (3V3_DIGITAL) ---------------
inline constexpr int I2cScl = 50;
inline constexpr int I2cSda = 51;
inline constexpr int EepromAddr = 0x50;   // A0=A1=A2=GND
inline constexpr int Max17048Addr = 0x36;
inline constexpr int Bq25896Addr = 0x6B;

// ---- Battery alert: PMIC_INT + MAX17048_ALERT on one open-drain line ----
// Pad 13, LP-capable, not a strapping pin (WROOM-3 datasheet v0.7, Table 4-1).
// -1 falls back to polling alone.
inline constexpr int PowerAlert = 7;

// ---- I2S to the ADAU1452, which is master of both clocks ------------
inline constexpr int I2sBclk = 46; // BCLK_OUT1 / BCLK_IN2
inline constexpr int I2sWs   = 47; // LRCLK_OUT1 / LRCLK_IN2
inline constexpr int I2sDout = 48; // to SDATA_IN2 (via R50)
inline constexpr int I2sDin  = 42; // from SDATA_OUT1 (via R40)

// ---- FSC-BT1058 -----------------------------------------------------
inline constexpr int BtUartRx  = 10; // <- module UART_TX (pin 13)
inline constexpr int BtUartTx  = 11; // -> module UART_RX (pin 14)
inline constexpr int BtSysCtrl = 12; // SYS_CTRL (pin 34), pull-down
inline constexpr int BtReset   = 13; // RESET (pin 8)

// ---- Second SPI flash U3, W25Q128, not used by this firmware yet ----
inline constexpr int ExtFlashCs   = 52;
inline constexpr int ExtFlashSclk = 53;
inline constexpr int ExtFlashMosi = 54;
inline constexpr int ExtFlashMiso = 55;

} // namespace board::pins
