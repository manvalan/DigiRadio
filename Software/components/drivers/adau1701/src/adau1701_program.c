/**
 * @file    adau1701_program.c
 * @brief   SigmaStudio default download for DigiRadio ADAU1701.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */

#include "SigmaStudioFW.h"
#include "DigiRadio_IC_1.h"

void adau1701_run_default_download(void)
{
    default_download_IC_1();
}
