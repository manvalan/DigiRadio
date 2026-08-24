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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char* kTag = "SigmaStudioFW";
static i2c_master_dev_handle_t s_dev = NULL;
static SemaphoreHandle_t s_lock = NULL;

void sigma_studio_bind_i2c(int port, unsigned char addr7)
{
    (void)port;
    (void)addr7;
}

void sigma_studio_set_device(void* i2cDevHandle)
{
    s_dev = (i2c_master_dev_handle_t)i2cDevHandle;
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }
}

void sigma_studio_lock(void)
{
    if (s_lock != NULL) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

void sigma_studio_unlock(void)
{
    if (s_lock != NULL) {
        xSemaphoreGive(s_lock);
    }
}

/*
 * The ADAU1701 memory map is word-indexed with a region-dependent word
 * width (Param RAM = 4 bytes, Program RAM = 5 bytes, Control regs =
 * 2 bytes) — confirmed by DigiRadio's own generated export
 * (DigiRadio_IC_1.h: PROGRAM_ADDR_IC_1=1024/PROGRAM_SIZE_IC_1=5120,
 * PARAM_ADDR_IC_1=0/PARAM_SIZE_IC_1=4096; DigiRadio_IC_1_REG.h:
 * REG_COREREGISTER_IC_1_ADDR=0x81C) and independently by the
 * ADAU1701-TCPi-ESP32 reference project's directWrite(). Chunk
 * boundaries must land on whole words, or the address advance for the
 * next I2C transaction (and the data itself, mid-word) is wrong.
 */
static unsigned int sigmaWordSize(unsigned int address)
{
    if (address >= 0x0400U && address <= 0x07FFU) {
        return 5U;
    }
    if (address >= 0x0800U) {
        return 2U;
    }
    return 4U;
}

/*
 * The boot-time program/param replay was previously fire-and-forget: the
 * i2c_master_transmit result was discarded outright, so a single transient
 * NACK anywhere in the hundreds of chunked writes that make up a DSP
 * program load silently left that one chunk (a gain, a filter, a mux)
 * at its power-on-reset RAM contents instead of the SigmaStudio-designed
 * value -- indistinguishable at the time from a correct load, but capable
 * of producing exactly a persistent, localized audio artifact rather than
 * gross silence. Give it the same retry-on-NACK reliability the runtime
 * safeload path already got in sigma_i2c_write (see its comment) and make
 * failure observable instead of silent.
 */
static int sigmaTransmitChunk(unsigned int addr, const ADI_REG_TYPE* payload,
                              unsigned int chunkBytes)
{
    unsigned char buf[2U + 64U];
    buf[0] = (unsigned char)((addr >> 8) & 0xFFU);
    buf[1] = (unsigned char)(addr & 0xFFU);
    memcpy(buf + 2U, payload, chunkBytes);

    static const int kMaxAttempts = 3;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (i2c_master_transmit(s_dev, buf, (size_t)(2U + chunkBytes), 1000) ==
            ESP_OK) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return -1;
}

int SIGMA_WRITE_REGISTER_BLOCK(unsigned char devAddress,
                               unsigned int address,
                               unsigned int length,
                               ADI_REG_TYPE* pData)
{
    (void)devAddress;
    if (s_dev == NULL || pData == NULL || length == 0U) {
        return -1;
    }

    enum { kChunkBytesMax = 64U };
    const unsigned int wordSize = sigmaWordSize(address);
    const unsigned int wordsPerChunk = kChunkBytesMax / wordSize;
    const unsigned int chunkBytes = wordsPerChunk * wordSize;

    unsigned int addr = address;
    unsigned int remaining = length;
    ADI_REG_TYPE* cursor = pData;

    while (remaining > 0U) {
        const unsigned int chunk =
            remaining > chunkBytes ? chunkBytes : remaining;
        const unsigned int words = chunk / wordSize;
        if (sigmaTransmitChunk(addr, cursor, chunk) != 0) {
            return -1;
        }
        addr += words;
        cursor += chunk;
        remaining -= chunk;
    }
    return 0;
}

int sigma_verify_block(unsigned int address, const ADI_REG_TYPE* expected,
                       unsigned int length)
{
    if (s_dev == NULL || expected == NULL || length == 0U) {
        return -1;
    }

    enum { kChunkBytesMax = 64U };
    const unsigned int wordSize = sigmaWordSize(address);
    const unsigned int wordsPerChunk = kChunkBytesMax / wordSize;
    const unsigned int chunkBytes = wordsPerChunk * wordSize;

    unsigned int addr = address;
    unsigned int remaining = length;
    const ADI_REG_TYPE* cursor = expected;

    while (remaining > 0U) {
        const unsigned int chunk =
            remaining > chunkBytes ? chunkBytes : remaining;
        const unsigned int words = chunk / wordSize;
        unsigned char readBack[kChunkBytesMax];
        if (sigma_i2c_read(addr, readBack, chunk) != 0) {
            return -1;
        }
        if (memcmp(readBack, cursor, chunk) != 0) {
            return -1;
        }
        addr += words;
        cursor += chunk;
        remaining -= chunk;
    }
    return 0;
}

int sigma_i2c_read(unsigned int reg, unsigned char* data, unsigned int length)
{
    if (s_dev == NULL || data == NULL || length == 0U) {
        return -1;
    }

    unsigned char addrBuf[2U];
    addrBuf[0] = (unsigned char)((reg >> 8) & 0xFFU);
    addrBuf[1] = (unsigned char)(reg & 0xFFU);

    const esp_err_t err = i2c_master_transmit_receive(
        s_dev, addrBuf, sizeof(addrBuf), data, (size_t)length, 1000);
    return err == ESP_OK ? 0 : -1;
}

static int sigma_i2c_write(unsigned int reg, const unsigned char* data,
                           unsigned char length)
{
    if (s_dev == NULL || data == NULL || length == 0U) {
        return -1;
    }

    unsigned char buf[2U + 5U];
    if ((size_t)length + 2U > sizeof(buf)) {
        return -1;
    }
    buf[0] = (unsigned char)((reg >> 8) & 0xFFU);
    buf[1] = (unsigned char)(reg & 0xFFU);
    memcpy(buf + 2U, data, length);

    // Long safeload sequences (e.g. one EQ band = 5 params x 2 writes +
    // trigger) chain dozens of back-to-back transactions; a single
    // transient NACK anywhere in that chain used to abort the whole
    // sequence. Retry a few times before giving up -- cheap and matches
    // how every other bus driver in this codebase tolerates bus noise.
    static const int kMaxAttempts = 3;
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        err = i2c_master_transmit(s_dev, buf, (size_t)(2U + length), 1000);
        if (err == ESP_OK) {
            return 0;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return -1;
}

/*
 * Safeload Data registers are 5 bytes wide (fixed 0x00 qualifier byte +
 * full sign-extended 32-bit value, MSB first) — confirmed against the
 * ADAU1701-TCPi-ESP32 reference project's safeloadChunk(), which sends
 * the identical 5-byte layout. `fixpoint` is a full-range Q8.23
 * two's-complement value (core::floatToFixpoint823): the previous
 * 4-byte payload here dropped the top (sign) byte entirely, silently
 * corrupting every negative coefficient (e.g. biquad a1/b1, routinely
 * negative for peaking/shelving EQ bands).
 */
static int sigma_write_fixpoint_reg(unsigned int reg, int fixpoint)
{
    unsigned char payload[5U];
    payload[0] = 0U;
    payload[1] = (unsigned char)(((unsigned int)fixpoint >> 24) & 0xFFU);
    payload[2] = (unsigned char)(((unsigned int)fixpoint >> 16) & 0xFFU);
    payload[3] = (unsigned char)(((unsigned int)fixpoint >> 8) & 0xFFU);
    payload[4] = (unsigned char)((unsigned int)fixpoint & 0xFFU);
    return sigma_i2c_write(reg, payload, 5U);
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

/*
 * Diagnostic only, mirrors the boot-replay read-back in
 * SIGMA_WRITE_REGISTER_BLOCK: a safeload can ACK every transaction and
 * still not land as intended (address/data written to the wrong Param RAM
 * slot, a stale value left from a previous session's committed-but-later-
 * garbled write, etc). This is the runtime path applied on every EQ/gain
 * change and on the stored-profile replay at boot -- unlike the one-time
 * program load, it was previously unverified. Log-only: some callers pass
 * addresses this can't independently confirm are readable Param RAM (vs.
 * a write-only control register), so a mismatch here is a strong signal,
 * not a hard boot/apply-time failure.
 */
static void sigmaVerifyParam(unsigned int paramAddr, int fixpoint)
{
    unsigned char readBack[4U];
    if (sigma_i2c_read(paramAddr, readBack, sizeof(readBack)) != 0) {
        ESP_LOGW(kTag, "safeload verify: read-back failed for param 0x%04X",
                 paramAddr);
        return;
    }
    const int actual = (int)(((unsigned int)readBack[0] << 24) |
                             ((unsigned int)readBack[1] << 16) |
                             ((unsigned int)readBack[2] << 8) |
                             (unsigned int)readBack[3]);
    if (actual != fixpoint) {
        ESP_LOGW(kTag,
                 "safeload verify: param 0x%04X mismatch, wrote %d read %d",
                 paramAddr, fixpoint, actual);
    }
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

    if (sigma_trigger_safeload() != 0) {
        return -1;
    }

    for (unsigned char i = 0U; i < count; ++i) {
        sigmaVerifyParam(paramAddrs[i], fixpoints[i]);
    }
    return 0;
}

int sigma_safeload_raw_block(unsigned char count, const unsigned int* paramAddrs,
                             const unsigned char* rawWords)
{
    if (count == 0U || count > 5U || paramAddrs == NULL || rawWords == NULL) {
        return -1;
    }

    for (unsigned char i = 0U; i < count; ++i) {
        const unsigned int dataReg =
            ADAU1701_SAFELOAD_DATA_BASE + (unsigned int)i;
        const unsigned int addrReg =
            ADAU1701_SAFELOAD_ADDR_BASE + (unsigned int)i;

        unsigned char payload[5U];
        payload[0] = 0U;
        memcpy(payload + 1U, rawWords + ((unsigned int)i * 4U), 4U);
        if (sigma_i2c_write(dataReg, payload, 5U) != 0) {
            return -1;
        }
        if (sigma_write_param_addr(addrReg, paramAddrs[i]) != 0) {
            return -1;
        }
    }

    return sigma_trigger_safeload();
}
