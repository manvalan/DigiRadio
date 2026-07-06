/**
 * @file    board_pins.hpp
 * @brief   DigiRadio ESP32-S3 board pin map — single source of truth.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * These values MUST match the schematic and the manual's GPIO tables
 * (docs/manual, Section "Inter-chip connections") exactly. This header
 * belongs to the imperative shell; it does not go in the pure core.
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#include <cstdint>

namespace board::pins {

// ---- System / USB ---------------------------------------------------
inline constexpr int UsbDMinus   = 19;  // native USB D-
inline constexpr int UsbDPlus    = 20;  // native USB D+
inline constexpr int BootButton  = 0;   // GPIO0 strapping / boot button

// ---- Si4684 tuner (SPI) ---------------------------------------------
inline constexpr int Si4684Cs    = 8;   // SSB / chip select (Si pin 29), active-low
inline constexpr int Si4684Miso  = 9;   // MISO (Si pin 32)
inline constexpr int Si4684Mosi  = 12;  // MOSI (Si pin 31)
inline constexpr int Si4684Sclk  = 13;  // SCLK (Si pin 30)
inline constexpr int Si4684Rstb  = 38;  // RSTB# (pin 4), active-low, ext pull-down
inline constexpr int Si4684Intb  = 39;  // INTB# (pin 3), active-low, ext pull-up

// ---- ADAU1701 DSP ---------------------------------------------------
inline constexpr int Adau1701Reset = 47; // RESET# (pin 5), active-low, ext pull-up
inline constexpr int Adau1701Sda   = 4;  // I2C SDA (control bus)
inline constexpr int Adau1701Scl   = 5;  // I2C SCL (control bus)
inline constexpr int Adau1701Addr  = 0x34; // 7-bit I2C addr (ADDR0=ADDR1=GND)

// ---- 24AA025E48 EEPROM (shared I2C bus; factory EUI-48 unique ID) ----
// A0=GND, A1=3V3 -> 7-bit addr 0x52 (SOT-23 package has no A2 pin).
inline constexpr int Eeprom24aaAddr = 0x52;

// ---- ESP32 I2S source into the ADAU mixer (ADAU is I2S master) ------
inline constexpr int I2sBclk    = 6;   // BCLK  in from ADAU master
inline constexpr int I2sLrclk   = 7;   // LRCLK in from ADAU master
inline constexpr int I2sDataOut = 16;  // SDATA_IN1 out -> ADAU MP1 (pin 10); GPIO16 = module pin 9

// ---- FSC-BT1035 Bluetooth (UART + control) --------------------------
inline constexpr int Bt1035SysCtl = 15; // SYS_CTL (pin 34)
inline constexpr int Bt1035Cts    = 21; // BT_CTS  (pin 15)
inline constexpr int Bt1035Rts    = 14; // BT_RTS  (pin 16)
inline constexpr int Bt1035UartTx = 40; // ESP32 TX -> BT_RX (pin 14)
inline constexpr int Bt1035UartRx = 41; // ESP32 RX <- BT_TX (pin 13)
inline constexpr int Bt1035Reset  = 17; // RESET   (pin 8)

// ---- Audio bus (I2S, chip-to-chip; ADAU1701 is master) --------------
// Not ESP32 GPIO (except the pending ESP32 source lines above):
//   SDATA_IN0  Si4684 -> ADAU MP0  (pin 11)
//   SDATA_IN1  ESP32  -> ADAU MP1  (pin 10)
//   SDATA_OUT0 ADAU MP6 (pin 15)   -> BT1035 pin 5
//   LRCLK      ADAU MP4(8)+MP10(16)-> BT1035 pin 7
//   BCLK       ADAU MP5(9)+MP11(19)-> BT1035 pin 4

} // namespace board::pins
