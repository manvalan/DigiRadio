/**
 * @file    test_firmware.hpp
 * @brief   Minimal DigiRadio hardware test firmware entrypoint.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

namespace test_firmware {

/**
 * Sequentially boots and smoke-tests each companion chip (ADAU1701,
 * Si4684, BT1035) with serial log output, then halts. Entered from
 * app_main() when CONFIG_TEST_FIRMWARE is enabled, instead of the
 * normal boot path.
 */
void runTestFirmware();

} // namespace test_firmware
