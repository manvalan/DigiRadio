/**
 * @file    commands.hpp
 * @brief   Serial console commands of the bring-up tools.
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

/** "run" and "run nobt": the hardware test app. */
void registerTestCommands();
/** Every register, bus and pin command: the debug monitor app. */
void registerMonitorCommands();
/** Starts the REPL on the USB console. */
void startConsole(const char* prompt);

} // namespace bu
