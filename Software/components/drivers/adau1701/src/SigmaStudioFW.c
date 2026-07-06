/**
 * @file    SigmaStudioFW.c
 * @brief   SigmaStudio SIGMA_WRITE_REGISTER_BLOCK for ESP-IDF I2C.
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

#include "driver/i2c_master.h"

#include <string.h>

static i2c_master_dev_handle_t s_dev = NULL;

void sigma_studio_bind_i2c(int port, unsigned char addr7)
{
    (void)port;
    (void)addr7;
}

void sigma_studio_set_device(void* i2cDevHandle)
{
    s_dev = (i2c_master_dev_handle_t)i2cDevHandle;
}

void SIGMA_WRITE_REGISTER_BLOCK(unsigned char devAddress,
                                unsigned int address,
                                unsigned char length,
                                ADI_REG_TYPE* pData)
{
    (void)devAddress;
    if (s_dev == NULL || pData == NULL || length == 0U) {
        return;
    }

    enum { kChunk = 64U };
    unsigned int addr = address;
    unsigned char remaining = length;
    ADI_REG_TYPE* cursor = pData;

    while (remaining > 0U) {
        const unsigned char chunk =
            remaining > kChunk ? kChunk : remaining;
        unsigned char buf[2U + 64U];
        buf[0] = (unsigned char)((addr >> 8) & 0xFFU);
        buf[1] = (unsigned char)(addr & 0xFFU);
        memcpy(buf + 2U, cursor, chunk);
        i2c_master_transmit(s_dev, buf, (size_t)(2U + chunk), 1000);
        addr += chunk;
        cursor += chunk;
        remaining = (unsigned char)(remaining - chunk);
    }
}
