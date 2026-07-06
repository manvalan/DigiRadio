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
 * @param    length      Payload length in bytes.
 * @param    pData       Payload bytes.
 */
void SIGMA_WRITE_REGISTER_BLOCK(unsigned char devAddress,
                                unsigned int address,
                                unsigned char length,
                                ADI_REG_TYPE* pData);

/**
 * @brief    Run the DigiRadio SigmaStudio default download sequence.
 */
void adau1701_run_default_download(void);

#ifdef __cplusplus
}
#endif
