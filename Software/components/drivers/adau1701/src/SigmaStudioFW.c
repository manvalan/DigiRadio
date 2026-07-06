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

static int sigma_i2c_write(unsigned int reg, const unsigned char* data,
                           unsigned char length)
{
    if (s_dev == NULL || data == NULL || length == 0U) {
        return -1;
    }

    unsigned char buf[2U + 4U];
    if ((size_t)length + 2U > sizeof(buf)) {
        return -1;
    }
    buf[0] = (unsigned char)((reg >> 8) & 0xFFU);
    buf[1] = (unsigned char)(reg & 0xFFU);
    memcpy(buf + 2U, data, length);
    const esp_err_t err =
        i2c_master_transmit(s_dev, buf, (size_t)(2U + length), 1000);
    return err == ESP_OK ? 0 : -1;
}

static int sigma_write_fixpoint_reg(unsigned int reg, int fixpoint)
{
    unsigned char payload[4U];
    payload[0] = 0U;
    payload[1] = (unsigned char)((fixpoint >> 16) & 0xFFU);
    payload[2] = (unsigned char)((fixpoint >> 8) & 0xFFU);
    payload[3] = (unsigned char)(fixpoint & 0xFFU);
    return sigma_i2c_write(reg, payload, 4U);
}

static int sigma_write_param_addr(unsigned int reg, unsigned int paramAddr)
{
    unsigned char payload[2U];
    payload[0] = (unsigned char)((paramAddr >> 8) & 0xFFU);
    payload[1] = (unsigned char)(paramAddr & 0xFFU);
    return sigma_i2c_write(reg, payload, 2U);
}

static int sigma_trigger_safeload(void)
{
    unsigned char payload[2U];
    payload[0] = (unsigned char)((ADAU1701_CORE_IST_TRIGGER >> 8) & 0xFFU);
    payload[1] = (unsigned char)(ADAU1701_CORE_IST_TRIGGER & 0xFFU);
    return sigma_i2c_write(ADAU1701_CORE_CONTROL_REG, payload, 2U);
}

int sigma_safeload_param(unsigned int paramAddr, int fixpoint)
{
    const unsigned int addrs[1U] = {paramAddr};
    const int values[1U] = {fixpoint};
    return sigma_safeload_block(1U, addrs, values);
}

int sigma_safeload_block(unsigned char count, const unsigned int* paramAddrs,
                         const int* fixpoints)
{
    if (count == 0U || count > 5U || paramAddrs == NULL || fixpoints == NULL) {
        return -1;
    }

    for (unsigned char i = 0U; i < count; ++i) {
        const unsigned int dataReg =
            ADAU1701_SAFELOAD_DATA_BASE + (unsigned int)i;
        const unsigned int addrReg =
            ADAU1701_SAFELOAD_ADDR_BASE + (unsigned int)i;
        if (sigma_write_fixpoint_reg(dataReg, fixpoints[i]) != 0) {
            return -1;
        }
        if (sigma_write_param_addr(addrReg, paramAddrs[i]) != 0) {
            return -1;
        }
    }

    return sigma_trigger_safeload();
}
