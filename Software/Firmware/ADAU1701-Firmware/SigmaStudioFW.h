/**
 * @file    SigmaStudioFW.h
 * @brief   Minimal SigmaStudio download helpers for ADAU1701 RAM boot.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Subset of the Analog Devices SigmaStudio export support library.
 *
 * @author  Michele Bigi
 * @date    2026-07-06
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char ADI_REG_TYPE;

/**
 * @brief    Bind the I2C port used by SIGMA_WRITE_REGISTER_BLOCK.
 *
 * @param    port       ESP-IDF I2C master port number.
 * @param    addr7      ADAU1701 7-bit I2C address (0x34 on DigiRadio).
 */
void sigma_studio_bind_i2c(int port, unsigned char addr7);

/** @brief Attach the I2C device handle created by Adau1701Driver. */
void sigma_studio_set_device(void* i2cDevHandle);

/**
 * @brief    Write a contiguous register/data block to the ADAU1701.
 *
 * @param    devAddress  SigmaStudio device address (0x68 write addr).
 * @param    address     16-bit target address in DSP memory map.
 * @param    length      Payload length in bytes (may exceed 255).
 * @param    pData       Payload bytes.
 */
void SIGMA_WRITE_REGISTER_BLOCK(unsigned char devAddress,
                                unsigned int address,
                                unsigned int length,
                                ADI_REG_TYPE* pData);

/** Safeload data register base (0x0810..0x0814). */
#define ADAU1701_SAFELOAD_DATA_BASE 0x0810U
/** Safeload address register base (0x0815..0x0819). */
#define ADAU1701_SAFELOAD_ADDR_BASE 0x0815U
/** DSP core control register (IST bit triggers safeload). */
#define ADAU1701_CORE_CONTROL_REG   0x081CU
/** IST bit value OR-ed into core control to commit safeload. */
#define ADAU1701_CORE_IST_TRIGGER   0x003CU

/**
 * @brief    Safeload one 8.23 fixpoint parameter (click-free update).
 *
 * @param    paramAddr  Parameter RAM address from DigiRadio_IC_1_PARAM.h.
 * @param    fixpoint   32-bit SigmaStudio fixpoint value.
 * @return   0 on success, non-zero on I2C failure.
 */
int sigma_safeload_param(unsigned int paramAddr, int fixpoint);

/**
 * @brief    Safeload up to five parameters in one IST transfer (e.g. biquad).
 *
 * @param    count      Number of pairs (1..5).
 * @param    paramAddrs Parameter RAM addresses.
 * @param    fixpoints  32-bit fixpoint values.
 * @return   0 on success, non-zero on I2C failure.
 */
int sigma_safeload_block(unsigned char count, const unsigned int* paramAddrs,
                         const int* fixpoints);

#ifdef __cplusplus
}
#endif
