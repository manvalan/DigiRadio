/**
 * @file    selftest.cpp
 * @brief   v2 board self test.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Register sources:
 * - BQ25896: TI SLUSC76C, section 9.4 (REG00-REG14).
 * - MAX17048: VCELL 0x02 (78.125 uV/LSB), SOC 0x04 (1/256 %), MODE 0x06,
 *   VERSION 0x08, CONFIG 0x0C (ALRT = bit 5 of the low byte), CRATE 0x16
 *   (0.208 %/h, signed), STATUS 0x1A. Same map as power_monitor.cpp; the data
 *   sheet is not in the repo yet.
 * - ADAU1452 Rev. D: PLL_CTRL0 0xF000, PLL_LOCK 0xF004, CORE_STATUS 0xF405,
 *   PANIC_FLAG 0xF427, PANIC_CODE 0xF428, SPDIF_TX_EN 0xF690.
 * - Si4684 AN649: POWER_UP 0x01, GET_PART_INFO 0x08, STATUS3 PUP_STATE.
 * - 24AA025E64: EUI-64 at 0xF8-0xFF, upper half write protected.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/selftest.hpp"

#include "v2bringup/hal.hpp"

#include "board_pins.hpp"

#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace bu {

namespace {

namespace pins = board::pins;

enum class St { Ok, Warn, Fail, Info, Skip };

struct Line {
    St st;
    std::string block;
    std::string text;
};

std::vector<Line> gLines;

const char* tag(St s)
{
    switch (s) {
    case St::Ok:   return "[ OK ]";
    case St::Warn: return "[WARN]";
    case St::Fail: return "[FAIL]";
    case St::Skip: return "[SKIP]";
    default:       return "[INFO]";
    }
}

void rep(St st, const char* block, const char* fmt, ...)
{
    std::array<char, 256> buf = {};
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf.data(), buf.size(), fmt, ap);
    va_end(ap);
    std::printf("%s %-10s %s\n", tag(st), block, buf.data());
    std::fflush(stdout);
    if (st != St::Info) {
        gLines.push_back({st, block, buf.data()});
    }
}

void section(const char* title)
{
    std::printf("\n=== %s ===\n", title);
    std::fflush(stdout);
}

// ---- BQ25896 ------------------------------------------------------------

const char* vbusStat(unsigned v)
{
    switch (v) {
    case 0: return "nessun ingresso";
    case 1: return "USB host SDP";
    case 2: return "adattatore 3.25 A";
    case 7: return "OTG";
    default: return "altro";
    }
}

const char* chrgStat(unsigned v)
{
    switch (v) {
    case 0: return "non in carica";
    case 1: return "pre-carica";
    case 2: return "carica rapida";
    default: return "carica terminata";
    }
}

struct BqAdc {
    bool ok = false;
    int vbatMv = 0;
    int vsysMv = 0;
    int vbusMv = 0;
    int ichgMa = 0;
    double tsPct = 0.0;
    bool vbusGood = false;
    std::uint8_t r0b = 0;
    std::uint8_t r0c = 0;
};

BqAdc bqConvert()
{
    BqAdc a;
    std::uint8_t r02 = 0;
    if (i2cRead8(kBqAddr, 0x02, r02) != ESP_OK
        || i2cWrite8(kBqAddr, 0x02, static_cast<std::uint8_t>(r02 | 0x80))
               != ESP_OK) {
        return a;
    }
    // CONV_START stays high during the conversion (up to 1 s, tCONV).
    for (int i = 0; i < 60; ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));
        if (i2cRead8(kBqAddr, 0x02, r02) == ESP_OK && (r02 & 0x80) == 0) {
            break;
        }
    }
    std::array<std::uint8_t, 5> r = {}; // REG0E..REG12
    std::uint8_t dummy = 0;
    if (i2cRead(kBqAddr, 0x0E, r.data(), r.size()) != ESP_OK
        || i2cRead8(kBqAddr, 0x0B, a.r0b) != ESP_OK
        || i2cRead8(kBqAddr, 0x0C, dummy) != ESP_OK // latched faults
        || i2cRead8(kBqAddr, 0x0C, a.r0c) != ESP_OK) { // present faults
        return a;
    }
    a.vbatMv = 2304 + (r[0] & 0x7F) * 20;
    a.vsysMv = 2304 + (r[1] & 0x7F) * 20;
    a.tsPct = 21.0 + (r[2] & 0x7F) * 0.465;
    a.vbusGood = (r[3] & 0x80) != 0;
    a.vbusMv = 2600 + (r[3] & 0x7F) * 100;
    a.ichgMa = (r[4] & 0x7F) * 50;
    a.ok = true;
    return a;
}

void printBqStatus(const BqAdc& a)
{
    std::printf("    VBUS %s (%u), %s, PG=%u, VSYS_STAT=%u, REG0B=0x%02X\n",
                a.vbusGood ? "presente" : "assente",
                (a.r0b >> 5) & 7, chrgStat((a.r0b >> 3) & 3),
                (a.r0b >> 2) & 1, a.r0b & 1, a.r0b);
    std::printf("    VBUS_STAT: %s\n", vbusStat((a.r0b >> 5) & 7));
    std::printf("    VBAT %d mV, VSYS %d mV, VBUS %d mV, ICHG misurata %d mA,"
                " TS %.1f %% di REGN\n",
                a.vbatMv, a.vsysMv, a.vbusMv, a.ichgMa, a.tsPct);
    std::printf("    REG0C guasti 0x%02X: watchdog=%u boost=%u chrg=%u bat=%u"
                " ntc=%u\n",
                a.r0c, a.r0c >> 7, (a.r0c >> 6) & 1, (a.r0c >> 4) & 3,
                (a.r0c >> 3) & 1, a.r0c & 7);
}

BqAdc gBq;

void testCharger()
{
    section("BQ25896 caricabatteria (0x6B)");
    std::uint8_t r14 = 0;
    if (i2cRead8(kBqAddr, 0x14, r14) != ESP_OK) {
        rep(St::Fail, "BQ25896", "non risponde su I2C");
        return;
    }
    const unsigned pn = (r14 >> 3) & 7;
    rep(pn == 0 ? St::Ok : St::Warn, "BQ25896",
        "REG14=0x%02X PN=%u (%s) DEV_REV=%u", r14, pn,
        pn == 0 ? "bq25896" : "atteso 000", r14 & 3);

    // Watchdog off first, or after 40 s REG04 returns to 2048 mA.
    std::uint8_t r07 = 0;
    if (i2cRead8(kBqAddr, 0x07, r07) == ESP_OK
        && i2cWrite8(kBqAddr, 0x07, static_cast<std::uint8_t>(r07 & ~0x30))
               == ESP_OK) {
        rep(St::Ok, "BQ25896", "watchdog I2C disattivato (REG07 0x%02X->0x%02X)",
            r07, r07 & ~0x30);
    } else {
        rep(St::Fail, "BQ25896", "scrittura REG07 fallita");
    }

    std::uint8_t r04 = 0;
    const auto ichg = static_cast<std::uint8_t>(kTestChargeMa / 64);
    std::uint8_t back = 0;
    if (i2cRead8(kBqAddr, 0x04, r04) == ESP_OK
        && i2cWrite8(kBqAddr, 0x04,
                     static_cast<std::uint8_t>((r04 & 0x80) | ichg)) == ESP_OK
        && i2cRead8(kBqAddr, 0x04, back) == ESP_OK
        && (back & 0x7F) == ichg) {
        rep(St::Ok, "BQ25896", "corrente di carica %d mA (era %d mA)",
            (back & 0x7F) * 64, (r04 & 0x7F) * 64);
    } else {
        rep(St::Fail, "BQ25896", "impostazione ICHG non riuscita (letto 0x%02X)",
            back);
    }

    std::array<std::uint8_t, 4> r = {};
    if (i2cRead(kBqAddr, 0x00, r.data(), r.size()) == ESP_OK) {
        std::uint8_t r06 = 0;
        (void)i2cRead8(kBqAddr, 0x06, r06);
        std::printf("    IINLIM %d mA, EN_ILIM=%u, CHG_CONFIG=%u, SYS_MIN %d mV,"
                    " VREG %d mV\n",
                    100 + (r[0] & 0x3F) * 50, (r[0] >> 6) & 1, (r[3] >> 4) & 1,
                    3000 + ((r[3] >> 1) & 7) * 100, 3840 + (r06 >> 2) * 16);
    }

    gBq = bqConvert();
    if (!gBq.ok) {
        rep(St::Fail, "BQ25896", "conversione ADC non riuscita");
        return;
    }
    printBqStatus(gBq);
    rep(gBq.r0c == 0 ? St::Ok : St::Warn, "BQ25896",
        "VBAT %d mV, VSYS %d mV, %s, guasti 0x%02X", gBq.vbatMv, gBq.vsysMv,
        chrgStat((gBq.r0b >> 3) & 3), gBq.r0c);
}

// ---- MAX17048 -----------------------------------------------------------

struct Gauge {
    bool ok = false;
    std::uint16_t version = 0, vcell = 0, soc = 0, mode = 0, config = 0,
                  crate = 0, status = 0, vreset = 0;
};

Gauge readGauge()
{
    Gauge g;
    g.ok = i2cRead16(kFgAddr, 0x08, g.version) == ESP_OK
           && i2cRead16(kFgAddr, 0x02, g.vcell) == ESP_OK
           && i2cRead16(kFgAddr, 0x04, g.soc) == ESP_OK
           && i2cRead16(kFgAddr, 0x06, g.mode) == ESP_OK
           && i2cRead16(kFgAddr, 0x0C, g.config) == ESP_OK
           && i2cRead16(kFgAddr, 0x16, g.crate) == ESP_OK
           && i2cRead16(kFgAddr, 0x18, g.vreset) == ESP_OK
           && i2cRead16(kFgAddr, 0x1A, g.status) == ESP_OK;
    return g;
}

int gaugeMv(const Gauge& g)
{
    return static_cast<int>((g.vcell * 78125ULL) / 1000000ULL);
}

void printGaugeRegs(const Gauge& g)
{
    std::printf("    VERSION 0x%04X, VCELL %d mV, SOC %.2f %%, CRATE %.2f %%/h\n",
                g.version, gaugeMv(g), g.soc / 256.0,
                static_cast<std::int16_t>(g.crate) * 0.208);
    std::printf("    MODE 0x%04X, CONFIG 0x%04X (RCOMP 0x%02X, ALRT=%u,"
                " ATHD %u %%), VRESET/ID 0x%04X, STATUS 0x%04X (RI=%u)\n",
                g.mode, g.config, g.config >> 8, (g.config >> 5) & 1,
                32 - (g.config & 0x1F), g.vreset, g.status,
                (g.status >> 8) & 1);
}

void testGauge()
{
    section("MAX17048 fuel gauge (0x36)");
    const Gauge g = readGauge();
    if (!g.ok) {
        rep(St::Fail, "MAX17048", "non risponde su I2C");
        return;
    }
    printGaugeRegs(g);
    rep((g.version & 0xFFF0) == 0x0010 ? St::Ok : St::Warn, "MAX17048",
        "VERSION 0x%04X, VCELL %d mV, SOC %.1f %%", g.version, gaugeMv(g),
        g.soc / 256.0);
    if (gBq.ok) {
        const int diff = std::abs(gaugeMv(g) - gBq.vbatMv);
        rep(diff <= 150 ? St::Ok : St::Warn, "MAX17048",
            "VCELL contro VBAT del BQ25896: differenza %d mV", diff);
    }
}

// ---- EEPROM -------------------------------------------------------------

void testEeprom()
{
    section("24AA025E64 EEPROM (0x50)");
    std::array<std::uint8_t, 8> eui = {};
    if (i2cRead(kEeAddr, 0xF8, eui.data(), eui.size()) != ESP_OK) {
        rep(St::Fail, "EEPROM", "non risponde su I2C");
        return;
    }
    const bool blank = eui[0] == 0xFF && eui[1] == 0xFF && eui[2] == 0xFF;
    rep(blank ? St::Warn : St::Ok, "EEPROM",
        "EUI-64 %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X", eui[0], eui[1],
        eui[2], eui[3], eui[4], eui[5], eui[6], eui[7]);

    constexpr std::uint8_t kCell = 0x7F; // last byte of the writable half
    std::uint8_t orig = 0;
    std::uint8_t back = 0;
    bool ok = i2cRead8(kEeAddr, kCell, orig) == ESP_OK;
    const auto test = static_cast<std::uint8_t>(~orig);
    ok = ok && i2cWrite8(kEeAddr, kCell, test) == ESP_OK;
    vTaskDelay(pdMS_TO_TICKS(10)); // write cycle 5 ms max
    ok = ok && i2cRead8(kEeAddr, kCell, back) == ESP_OK && back == test;
    (void)i2cWrite8(kEeAddr, kCell, orig);
    vTaskDelay(pdMS_TO_TICKS(10));
    std::uint8_t restored = 0;
    (void)i2cRead8(kEeAddr, kCell, restored);
    rep(ok && restored == orig ? St::Ok : St::Fail, "EEPROM",
        "scrittura/rilettura cella 0x7F: scritto 0x%02X letto 0x%02X,"
        " ripristinato 0x%02X", test, back, restored);
}

// ---- PWR_ALERT ----------------------------------------------------------

void clearAlertSources()
{
    std::uint8_t f = 0;
    (void)i2cRead8(kBqAddr, 0x0C, f);
    (void)i2cRead8(kBqAddr, 0x0C, f);
    std::uint16_t cfg = 0;
    if (i2cRead16(kFgAddr, 0x0C, cfg) == ESP_OK && (cfg & 0x20) != 0) {
        (void)i2cWrite16(kFgAddr, 0x0C, static_cast<std::uint16_t>(cfg & ~0x20));
    }
}

void testAlert()
{
    section("PWR_ALERT (GPIO7)");
    if (!alertArm()) {
        rep(St::Fail, "ALERT", "interrupt su GPIO%d non installato",
            pins::PowerAlert);
        return;
    }
    clearAlertSources();
    vTaskDelay(pdMS_TO_TICKS(5));
    const int level = alertLevel();
    rep(level == 1 ? St::Ok : St::Fail, "ALERT",
        "livello a riposo %d (atteso 1, pull-up R11)", level);
    if (!gBq.ok || !gBq.vbusGood) {
        rep(St::Skip, "ALERT",
            "impulso INT non provato: collega l'USB e rilancia il test");
        return;
    }
    // A charge status change makes the BQ25896 pulse INT (about 256 us).
    std::uint8_t r03 = 0;
    alertClearCount();
    if (i2cRead8(kBqAddr, 0x03, r03) != ESP_OK) {
        rep(St::Fail, "ALERT", "lettura REG03 fallita");
        return;
    }
    (void)i2cWrite8(kBqAddr, 0x03, static_cast<std::uint8_t>(r03 & ~0x10));
    vTaskDelay(pdMS_TO_TICKS(500));
    (void)i2cWrite8(kBqAddr, 0x03, static_cast<std::uint8_t>(r03 | 0x10));
    vTaskDelay(pdMS_TO_TICKS(500));
    const int edges = alertEdges();
    clearAlertSources();
    rep(edges > 0 ? St::Ok : St::Warn, "ALERT",
        "carica spenta e riaccesa: %d fronti di discesa su GPIO7", edges);
}

// ---- ADAU1452 -----------------------------------------------------------

bool coreRunning(std::uint16_t status)
{
    const unsigned s = status & 7U;
    return s == 1U || s == 3U || s == 4U;
}

const char* coreName(std::uint16_t status)
{
    switch (status & 7U) {
    case 0: return "non attivo";
    case 1: return "in esecuzione";
    case 2: return "in pausa";
    case 3: return "in attesa tra campioni";
    case 4: return "in stallo breve";
    default: return "riservato";
    }
}

bool gDspRunning = false;

bool waitSelfBoot(std::uint16_t& status, std::uint16_t& panic, int& ms)
{
    const int64_t t0 = esp_timer_get_time();
    for (ms = 0; ms < 4000; ms = static_cast<int>(
                                (esp_timer_get_time() - t0) / 1000)) {
        if (adauReadReg(0xF405, status) != ESP_OK
            || adauReadReg(0xF427, panic) != ESP_OK) {
            return false;
        }
        if (coreRunning(status) || panic != 0) {
            return coreRunning(status);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

void testDspSpiFallback()
{
    // No program: only PLL_CTRL0..PLL_LOCK are reachable before the PLL locks.
    adauReset(false);
    adauToggleCs3();
    std::uint16_t back = 0;
    const bool ok = adauWriteReg(0xF000, 0x0060) == ESP_OK
                    && adauReadReg(0xF000, back) == ESP_OK && back == 0x0060;
    rep(ok ? St::Ok : St::Fail, "ADAU1452",
        "SELFBOOT basso: scritto PLL_CTRL0=0x0060, riletto 0x%04X%s", back,
        ok ? " (SPI in scrittura e lettura funziona)" : "");
    adauReset(true); // back to the self boot attempt for later tests
    adauToggleCs3();
}

void testDsp()
{
    section("ADAU1452 DSP (SPI, CS=GPIO20, SELFBOOT=GPIO22)");
    gDspRunning = false;
    adauReset(true);
    adauToggleCs3();
    rep(St::Info, "ADAU1452", "reset con SELFBOOT alto, 3 impulsi su SS");
    std::uint16_t status = 0;
    std::uint16_t panic = 0;
    int ms = 0;
    const bool running = waitSelfBoot(status, panic, ms);
    std::uint16_t lock = 0;
    std::uint16_t pll = 0;
    (void)adauReadReg(0xF004, lock);
    (void)adauReadReg(0xF000, pll);
    if (status == 0xFFFF && panic == 0xFFFF) {
        rep(St::Fail, "ADAU1452",
            "legge sempre 0xFFFF: MISO alto, chip non alimentato o non in SPI");
    }
    if (running) {
        gDspRunning = true;
        rep(St::Ok, "ADAU1452",
            "self boot riuscito in %d ms: core %s, PLL_LOCK=%u, PLL_CTRL0=0x%04X",
            ms, coreName(status), lock & 1, pll);
        return;
    }
    std::uint16_t code = 0;
    (void)adauReadReg(0xF428, code);
    rep(St::Fail, "ADAU1452",
        "nessun self boot (CORE_STATUS=0x%04X %s, PANIC_FLAG=0x%04X,"
        " PANIC_CODE=0x%04X, PLL_LOCK=%u)",
        status, coreName(status), panic, code, lock & 1);
    testDspSpiFallback();
}

// ---- Si4684 -------------------------------------------------------------

void testTuner()
{
    section("Si4684 sintonizzatore (SPI, CS=GPIO21)");
    siReset();
    std::array<std::uint8_t, 24> r = {};
    if (siReadReply(r.data(), 4) != ESP_OK) {
        rep(St::Fail, "Si4684", "SPI non disponibile");
        return;
    }
    rep((r[0] & 0x80) && (r[3] >> 6) == 0 ? St::Ok : St::Fail, "Si4684",
        "dopo il reset STATUS0=0x%02X STATUS3=0x%02X (CTS=%u, PUP_STATE=%u,"
        " atteso CTS=1 e 0)", r[0], r[3], r[0] >> 7, r[3] >> 6);

    // AN649 POWER_UP: crystal, IBIAS 72, 19.2 MHz, CTUN 0 (v2 defaults).
    constexpr std::uint32_t kXtal = 19200000U;
    const std::array<std::uint8_t, 16> pup = {
        0x01, 0x00, 0x17, 72,
        static_cast<std::uint8_t>(kXtal), static_cast<std::uint8_t>(kXtal >> 8),
        static_cast<std::uint8_t>(kXtal >> 16),
        static_cast<std::uint8_t>(kXtal >> 24),
        0x00, 0x10, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00};
    esp_err_t err = siCommand(pup.data(), pup.size(), r.data(), 4, 500);
    vTaskDelay(pdMS_TO_TICKS(20));
    (void)siReadReply(r.data(), 4);
    rep(err == ESP_OK && (r[3] >> 6) == 2 ? St::Ok : St::Fail, "Si4684",
        "POWER_UP: STATUS0=0x%02X STATUS3=0x%02X, PUP_STATE=%u (atteso 2,"
        " bootloader attivo)", r[0], r[3], r[3] >> 6);

    const std::array<std::uint8_t, 2> part = {0x08, 0x00};
    err = siCommand(part.data(), part.size(), r.data(), 23, 500);
    const unsigned id = r[8] | (r[9] << 8);
    rep(err == ESP_OK && id == 4684 ? St::Ok : St::Fail, "Si4684",
        "GET_PART_INFO: PART=%u CHIPREV=%u ROMID=%u (atteso 4684)", id, r[4],
        r[5]);
    rep(St::Info, "Si4684", "INTB#=%d", gpio_get_level(
        static_cast<gpio_num_t>(pins::Si4684Intb)));
}

// ---- I2S and TOSLINK ----------------------------------------------------

void testI2s()
{
    section("I2S (BCLK GPIO46, WS GPIO47, DOUT GPIO48, DIN GPIO42)");
    if (!gDspRunning) {
        rep(St::Skip, "I2S", "il DSP non gira: nessun clock da misurare");
        return;
    }
    long bclkEdges = 0, wsEdges = 0, dinEdges = 0;
    double bclk = 0, ws = 0, din = 0;
    measureEdges(pins::I2sBclk, 100, bclkEdges, bclk);
    measureEdges(pins::I2sWs, 100, wsEdges, ws);
    measureEdges(pins::I2sDin, 100, dinEdges, din);
    const double ratio = ws > 0 ? bclk / ws : 0;
    rep(ws > 40000 && ws < 50000 ? St::Ok : St::Fail, "I2S",
        "WS %.0f Hz (atteso 48000)", ws);
    rep(ratio > 60 && ratio < 68 ? St::Ok : St::Fail, "I2S",
        "BCLK %.0f Hz, %.1f BCLK per campione (atteso 64)", bclk, ratio);
    rep(dinEdges > 0 ? St::Ok : St::Warn, "I2S",
        "DIN dal DSP: %ld fronti in 100 ms%s", dinEdges,
        dinEdges > 0 ? "" : " (linea ferma: silenzio o pista interrotta)");
    long pl = 0, pr = 0, frames = 0;
    const esp_err_t err = i2sPeak(200, pl, pr, frames);
    rep(err == ESP_OK && frames > 5000 ? St::Ok : St::Fail, "I2S",
        "ESP32 slave: %ld frame in 200 ms, picco L %ld R %ld (24 bit)",
        frames, pl, pr);
    rep(St::Info, "I2S",
        "DOUT verso il DSP: prova a orecchio con 'i2s tone' nel monitor");
}

void testToslink()
{
    section("TOSLINK (J5, S/PDIF dal DSP)");
    if (!gDspRunning) {
        rep(St::Skip, "TOSLINK", "il DSP non gira");
        return;
    }
    std::uint16_t en = 0;
    std::uint16_t ctrl = 0;
    (void)adauReadReg(0xF690, en);
    (void)adauReadReg(0xF691, ctrl);
    rep((en & 1) ? St::Ok : St::Warn, "TOSLINK",
        "SPDIF_TX_EN=0x%04X SPDIF_TX_CTRL=0x%04X", en, ctrl);
    rep(St::Info, "TOSLINK",
        "verifica a vista: luce rossa nel connettore J5");
}

// ---- BT1058 -------------------------------------------------------------

bool btWaitBanner(std::string& banner)
{
    std::array<char, 128> buf = {};
    const int64_t t0 = esp_timer_get_time();
    int64_t lastData = 0;
    while (esp_timer_get_time() - t0 < 25000000LL) {
        const int n = btRead(buf.data(), buf.size(), 100);
        if (n > 0) {
            banner.append(buf.data(), static_cast<std::size_t>(n));
            lastData = esp_timer_get_time();
        } else if (lastData != 0
                   && esp_timer_get_time() - lastData > 2000000LL) {
            break;
        }
    }
    return !banner.empty();
}

bool btSweep()
{
    constexpr std::array<int, 7> kBauds = {9600,   19200,  38400, 57600,
                                           230400, 460800, 921600};
    for (const int b : kBauds) {
        btUartInit(b);
        const std::string r = btAt("AT", 400);
        if (!r.empty()) {
            rep(St::Warn, "BT1058", "risponde a %d baud: \"%s\"", b,
                printable(r.data(), r.size()).c_str());
            btUartInit(115200);
            return true;
        }
    }
    btUartInit(115200);
    return false;
}

void testBluetooth()
{
    section("FSC-BT1058 (UART GPIO10/11, SYS_CTRL GPIO12, RESET GPIO13)");
    rep(St::Info, "BT1058",
        "RESET# e SYS_CTRL bassi 2.5 s, RESET# alto, SYS_CTRL basso 200 ms,"
        " poi alto; attesa banner fino a 25 s");
    if (!btPowerUp()) {
        rep(St::Fail, "BT1058", "UART non inizializzata");
        return;
    }
    std::string banner;
    const bool got = btWaitBanner(banner);
    rep(got ? St::Ok : St::Warn, "BT1058", "banner: %s",
        got ? printable(banner.data(), std::min<std::size_t>(banner.size(), 200))
                  .c_str()
            : "nessun byte a 115200");
    std::string ok;
    for (int i = 0; i < 3 && ok.find("OK") == std::string::npos; ++i) {
        ok = btAt("AT", 1500);
    }
    if (ok.find("OK") == std::string::npos) {
        rep(St::Fail, "BT1058", "AT senza OK a 115200 (risposta \"%s\")",
            printable(ok.data(), ok.size()).c_str());
        if (!btSweep()) {
            rep(St::Fail, "BT1058", "nessuna risposta a nessun baud rate");
        }
        return;
    }
    rep(St::Ok, "BT1058", "AT -> OK");
    for (const char* q : {"AT+VER", "AT+NAME", "AT+MODCFG", "AT+AUXCFG",
                          "AT+I2SCFG"}) {
        const std::string r = btAt(q, 1500);
        rep(St::Info, "BT1058", "%s -> %s", q,
            printable(r.data(), r.size()).c_str());
    }
}

// ---- U3 -----------------------------------------------------------------

void testExtFlash()
{
    section("U3 W25Q128 (GPIO52-55)");
    std::uint8_t id[3] = {};
    const esp_err_t err = extFlashJedec(id);
    const bool ok = err == ESP_OK && id[0] == 0xEF && id[2] == 0x18;
    rep(ok ? St::Ok : St::Fail, "U3", "JEDEC ID %02X %02X %02X (atteso EF xx 18)",
        id[0], id[1], id[2]);
}

// ---- Bus scan and summary -----------------------------------------------

void testI2cScan()
{
    section("Bus I2C (SDA GPIO51, SCL GPIO50)");
    if (!i2cInit()) {
        rep(St::Fail, "I2C", "bus non inizializzato");
        return;
    }
    std::string found;
    bool bq = false, fg = false, ee = false;
    for (int a = 0x08; a < 0x78; ++a) {
        if (i2cProbe(static_cast<std::uint8_t>(a)) == ESP_OK) {
            char s[8];
            std::snprintf(s, sizeof s, " 0x%02X", a);
            found += s;
            bq |= a == kBqAddr;
            fg |= a == kFgAddr;
            ee |= a == kEeAddr;
        }
    }
    rep(bq && fg && ee ? St::Ok : St::Fail, "I2C",
        "trovati:%s (attesi 0x36 0x50 0x6B)",
        found.empty() ? " nessuno" : found.c_str());
}

void printEsp()
{
    section("ESP32-S31");
    esp_chip_info_t info = {};
    esp_chip_info(&info);
    std::printf("    IDF %s, core %d, rev %d.%d, heap libero %u byte\n",
                esp_get_idf_version(), info.cores, info.revision / 100,
                info.revision % 100,
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
}

int summary(int64_t t0)
{
    int ok = 0, warn = 0, fail = 0, skip = 0;
    std::printf("\n================ RIEPILOGO ================\n");
    for (const Line& l : gLines) {
        ok += l.st == St::Ok;
        warn += l.st == St::Warn;
        fail += l.st == St::Fail;
        skip += l.st == St::Skip;
        if (l.st != St::Ok) {
            std::printf("%s %-10s %s\n", tag(l.st), l.block.c_str(),
                        l.text.c_str());
        }
    }
    std::printf("OK %d, WARN %d, FAIL %d, SKIP %d in %.1f s\n", ok, warn, fail,
                skip, (esp_timer_get_time() - t0) / 1e6);
    std::printf("RISULTATO: %s\n", fail == 0 ? "SCHEDA OK" : "CI SONO GUASTI");
    std::printf("===========================================\n");
    std::fflush(stdout);
    return fail;
}

} // namespace

int runSelfTest(bool withBluetooth)
{
    gLines.clear();
    const int64_t t0 = esp_timer_get_time();
    std::printf("\n########## DigiRadio v2 - test hardware ##########\n");
    printEsp();
    testI2cScan();
    testCharger();
    testGauge();
    testEeprom();
    testAlert();

    section("Alimentazione analogica (ENABLE_ANALOG GPIO19)");
    const bool on = analogOn() && spiInit();
    rep(on ? St::Ok : St::Fail, "3V3_AUDIO",
        "ENABLE_ANALOG alto, SMODE basso, bus SPI a %d Hz", spiHz());
    if (on) {
        testDsp(); // first: until SS toggles it reads Si4684 traffic as I2C
        testTuner();
        testI2s();
        testToslink();
        if (withBluetooth) {
            testBluetooth();
        } else {
            rep(St::Skip, "BT1058", "saltato su richiesta");
        }
    }
    testExtFlash();
    return summary(t0);
}

void printCharger()
{
    const BqAdc a = bqConvert();
    if (!a.ok) {
        std::printf("BQ25896 non risponde\n");
        return;
    }
    std::array<std::uint8_t, 0x15> r = {};
    if (i2cRead(kBqAddr, 0x00, r.data(), 0x0C) == ESP_OK) {
        std::printf("REG00-0B:");
        for (int i = 0; i < 0x0C; ++i) {
            std::printf(" %02X", r[i]);
        }
        std::printf("\n");
    }
    std::printf("    ICHG impostata %d mA, IINLIM %d mA, watchdog %u\n",
                (r[4] & 0x7F) * 64, 100 + (r[0] & 0x3F) * 50,
                (r[7] >> 4) & 3);
    printBqStatus(a);
}

void printGauge()
{
    const Gauge g = readGauge();
    if (!g.ok) {
        std::printf("MAX17048 non risponde\n");
        return;
    }
    printGaugeRegs(g);
}

void printDspStatus()
{
    std::uint16_t core = 0, panic = 0, code = 0, lock = 0, pll = 0, spdif = 0;
    const esp_err_t err = adauReadReg(0xF405, core);
    (void)adauReadReg(0xF427, panic);
    (void)adauReadReg(0xF428, code);
    (void)adauReadReg(0xF004, lock);
    (void)adauReadReg(0xF000, pll);
    (void)adauReadReg(0xF690, spdif);
    std::printf("SPI %s; CORE_STATUS 0x%04X (%s), PANIC_FLAG 0x%04X,"
                " PANIC_CODE 0x%04X, PLL_LOCK %u, PLL_CTRL0 0x%04X,"
                " SPDIF_TX_EN 0x%04X\n",
                esp_err_to_name(err), core, coreName(core), panic, code,
                lock & 1, pll, spdif);
}

} // namespace bu
