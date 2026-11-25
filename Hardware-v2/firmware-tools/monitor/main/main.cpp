/**
 * @file    main.cpp
 * @brief   DigiRadio v2 debug monitor: interactive console for registers,
 *          buses, pins and power.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Starts with 3V3_AUDIO off and every audio pin floating; "power on" turns it
 * on. Nothing is written to any chip until a command asks for it.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/commands.hpp"
#include "v2bringup/hal.hpp"

#include <cstdio>

extern "C" void app_main()
{
    bu::i2cInit();
    bu::analogOff();
    std::printf("\nDigiRadio v2 debug monitor. 'help' per i comandi,"
                " 'power on' per accendere 3V3_AUDIO.\n");
    bu::registerMonitorCommands();
    bu::startConsole("v2> ");
}
