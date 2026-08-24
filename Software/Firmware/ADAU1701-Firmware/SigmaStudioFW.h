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
 * @brief    Acquire the process-wide ADAU1701 I2C lock.
 *
 * Guards a logical multi-transaction operation (e.g. a safeload burst)
 * against interleaving from another FreeRTOS task — individual
 * i2c_master_transmit calls are already serialised by the ESP-IDF I2C
 * driver, but a sequence of several transactions is not atomic without
 * this. Callers: Adau1701Driver safeload paths and the SigmaStudio TCP
 * bridge (components/net/SigmaStudioTcpServer).
 */
void sigma_studio_lock(void);

/** @brief Release the lock acquired by sigma_studio_lock(). */
void sigma_studio_unlock(void);

/**
 * @brief    Write a contiguous register/data block to the ADAU1701.
 *
 * Retries each I2C chunk up to 3 times on NACK, matching the reliability
 * already applied to the runtime safeload path (see sigma_i2c_write).
 *
 * @param    devAddress  SigmaStudio device address (0x68 write addr).
 * @param    address     16-bit target address in DSP memory map.
 * @param    length      Payload length in bytes (may exceed 255).
 * @param    pData       Payload bytes.
 * @return   0 on success, non-zero if any chunk failed after retries.
 */
int SIGMA_WRITE_REGISTER_BLOCK(unsigned char devAddress,
                               unsigned int address,
                               unsigned int length,
                               ADI_REG_TYPE* pData);

/**
 * @brief    Read a contiguous register/data block from the ADAU1701.
 *
 * @param    reg     16-bit source address in DSP memory map.
 * @param    data    Destination buffer.
 * @param    length  Number of bytes to read.
 * @return   0 on success, non-zero on I2C failure.
 */
int sigma_i2c_read(unsigned int reg, unsigned char* data, unsigned int length);

/**
 * @brief    Read back a previously written block and compare it byte-for-byte.
 *
 * Diagnostic aid for boot-time DSP program replay: a chunk that ACKed but
 * still landed wrong (or a register that doesn't hold the value it was
 * given) shows up here even though SIGMA_WRITE_REGISTER_BLOCK reported
 * success. Read-only — never aborts anything by itself, callers decide.
 *
 * @param    address   16-bit address the block was written to.
 * @param    expected  Bytes that were written.
 * @param    length    Payload length in bytes (may exceed 255).
 * @return   0 if the read-back matches, non-zero on mismatch or read failure.
 */
int sigma_verify_block(unsigned int address, const ADI_REG_TYPE* expected,
                       unsigned int length);

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

/**
 * @brief    Safeload up to five parameters from raw wire bytes (pass-through).
 *
 * Used by the SigmaStudio TCP bridge: unlike sigma_safeload_block(), which
 * reconstructs the 4-byte payload from a host int, this forwards each
 * 4-byte word exactly as received over the network — no endianness
 * reinterpretation.
 *
 * @param    count      Number of words (1..5).
 * @param    paramAddrs Parameter RAM addresses.
 * @param    rawWords   count*4 raw bytes, one 4-byte word per address.
 * @return   0 on success, non-zero on I2C failure.
 */
int sigma_safeload_raw_block(unsigned char count, const unsigned int* paramAddrs,
                             const unsigned char* rawWords);

#ifdef __cplusplus
}
#endif
