/**
 * @file    selftest.hpp
 * @brief   v2 board self test: every block in turn, report on the console.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */
#pragma once

namespace bu {

/** Charge current the self test programs into the BQ25896 (REG04). */
inline constexpr int kTestChargeMa = 256;

/**
 * Runs every test and prints one line per check plus a summary.
 * @param withBluetooth false skips the BT1058 (about 30 s).
 * @return number of failed checks.
 */
int runSelfTest(bool withBluetooth = true);

/** BQ25896: decoded status and ADC reading. */
void printCharger();
/** MAX17048: decoded registers. */
void printGauge();
/** ADAU1452: core, PLL and panic registers. */
void printDspStatus();

} // namespace bu
