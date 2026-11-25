/**
 * @file    main.cpp
 * @brief   DigiRadio v2 hardware test: runs every check at boot, then waits
 *          for "run" on the console.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/commands.hpp"
#include "v2bringup/selftest.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>

extern "C" void app_main()
{
    // Time to open the serial monitor after the USB port appears.
    for (int s = 3; s > 0; --s) {
        std::printf("DigiRadio v2 test hardware: parte tra %d s\n", s);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    bu::runSelfTest(true);
    std::printf("\nScrivi 'run' per ripetere il test, 'run nobt' senza BT1058.\n");
    bu::registerTestCommands();
    bu::startConsole("hwtest> ");
}
