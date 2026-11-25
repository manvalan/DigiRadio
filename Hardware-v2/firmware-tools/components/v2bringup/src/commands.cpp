/**
 * @file    commands.cpp
 * @brief   Serial console commands of the bring-up tools.
 *
 * DigiRadio firmware — https://github.com/manvalan/DigiRadio
 *
 * Copyright 2026 Michele Bigi
 * SPDX-License-Identifier: Apache-2.0
 *
 * Numbers take any C base: 0x50, 80, 0b... is not supported.
 *
 * @author  Michele Bigi
 * @date    2026-11-25
 */

#include "v2bringup/commands.hpp"

#include "v2bringup/hal.hpp"
#include "v2bringup/selftest.hpp"
#include "v2bringup/wireless.hpp"

#include "board_pins.hpp"

#include "driver/gpio.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace bu {

namespace {

namespace pins = board::pins;

bool num(const char* s, long& v)
{
    if (s == nullptr) {
        return false;
    }
    char* end = nullptr;
    v = std::strtol(s, &end, 0);
    return end != s && *end == '\0';
}

bool is(const char* a, const char* b)
{
    return a != nullptr && std::strcmp(a, b) == 0;
}

int usage(const char* text)
{
    std::printf("uso: %s\n", text);
    return 1;
}

int err(const char* what, esp_err_t e)
{
    std::printf("%s: %s\n", what, esp_err_to_name(e));
    return e == ESP_OK ? 0 : 1;
}

bool needAnalog()
{
    if (!analogIsOn()) {
        std::printf("3V3_AUDIO spento: prima 'power on'\n");
        return false;
    }
    return spiInit();
}

void hexdump(std::uint32_t base, const std::uint8_t* d, std::size_t n)
{
    for (std::size_t i = 0; i < n; i += 16) {
        std::printf("%04lX:", static_cast<unsigned long>(base + i));
        for (std::size_t j = i; j < n && j < i + 16; ++j) {
            std::printf(" %02X", d[j]);
        }
        std::printf("\n");
    }
}

// ---- run ----------------------------------------------------------------

int cmdRun(int argc, char** argv)
{
    const bool bt = !(argc > 1 && is(argv[1], "nobt"));
    runSelfTest(bt);
    return 0;
}

// ---- power / gpio / spi -------------------------------------------------

int cmdPower(int argc, char** argv)
{
    if (argc > 1 && is(argv[1], "on")) {
        const bool ok = analogOn() && spiInit();
        std::printf("3V3_AUDIO acceso%s\n", ok ? "" : " (SPI non pronto)");
    } else if (argc > 1 && is(argv[1], "off")) {
        analogOff();
        std::printf("3V3_AUDIO spento, pin audio in alta impedenza\n");
    } else if (argc > 1) {
        return usage("power [on|off]");
    }
    std::printf("ENABLE_ANALOG=%d, PWR_ALERT=%d\n",
                gpio_get_level(static_cast<gpio_num_t>(pins::EnableAnalog)),
                alertLevel());
    return 0;
}

int cmdGpio(int argc, char** argv)
{
    long pin = 0;
    if (argc < 2 || !num(argv[1], pin) || pin < 0 || pin > 61) {
        return usage("gpio <n> [0|1|in|up|down|float]");
    }
    const auto p = static_cast<gpio_num_t>(pin);
    if (argc > 2) {
        long lvl = 0;
        if (num(argv[2], lvl)) {
            gpio_set_level(p, lvl != 0);
            gpio_set_direction(p, GPIO_MODE_INPUT_OUTPUT);
            gpio_set_level(p, lvl != 0);
        } else if (is(argv[2], "in") || is(argv[2], "up")
                   || is(argv[2], "down") || is(argv[2], "float")) {
            gpio_set_direction(p, is(argv[2], "float") ? GPIO_MODE_DISABLE
                                                       : GPIO_MODE_INPUT);
            gpio_set_pull_mode(p, is(argv[2], "up")     ? GPIO_PULLUP_ONLY
                                  : is(argv[2], "down") ? GPIO_PULLDOWN_ONLY
                                                        : GPIO_FLOATING);
        } else {
            return usage("gpio <n> [0|1|in|up|down|float]");
        }
    }
    std::printf("GPIO%ld = %d\n", pin, gpio_get_level(p));
    return 0;
}

int cmdSpi(int argc, char** argv)
{
    long hz = 0;
    if (argc == 3 && is(argv[1], "hz") && num(argv[2], hz) && hz > 0) {
        spiSetHz(static_cast<int>(hz));
    } else if (argc != 1) {
        return usage("spi [hz <frequenza>]");
    }
    std::printf("SPI controllo a %d Hz\n", spiHz());
    return 0;
}

// ---- i2c ----------------------------------------------------------------

int i2cScan()
{
    for (int a = 0x08; a < 0x78; ++a) {
        if (i2cProbe(static_cast<std::uint8_t>(a)) == ESP_OK) {
            std::printf("  0x%02X\n", a);
        }
    }
    return 0;
}

int cmdI2c(int argc, char** argv)
{
    i2cInit();
    long addr = 0, reg = 0, n = 1;
    if (argc >= 2 && is(argv[1], "scan")) {
        return i2cScan();
    }
    if (argc < 4 || !num(argv[2], addr) || !num(argv[3], reg)) {
        return usage("i2c scan | i2c r <addr> <reg> [n] | i2c w <addr> <reg> <byte>...");
    }
    const auto a = static_cast<std::uint8_t>(addr);
    const auto r = static_cast<std::uint8_t>(reg);
    if (is(argv[1], "r")) {
        if (argc > 4 && (!num(argv[4], n) || n < 1 || n > 256)) {
            return usage("i2c r <addr> <reg> [1..256]");
        }
        std::array<std::uint8_t, 256> d = {};
        const esp_err_t e = i2cRead(a, r, d.data(), static_cast<std::size_t>(n));
        if (e == ESP_OK) {
            hexdump(r, d.data(), static_cast<std::size_t>(n));
        }
        return err("lettura", e);
    }
    if (is(argv[1], "w") && argc >= 5) {
        std::array<std::uint8_t, 32> d = {};
        std::size_t k = 0;
        for (int i = 4; i < argc && k < d.size(); ++i) {
            long b = 0;
            if (!num(argv[i], b)) {
                return usage("i2c w <addr> <reg> <byte>...");
            }
            d[k++] = static_cast<std::uint8_t>(b);
        }
        return err("scrittura", i2cWrite(a, r, d.data(), k));
    }
    return usage("i2c scan | i2c r <addr> <reg> [n] | i2c w <addr> <reg> <byte>...");
}

// ---- BQ25896 / MAX17048 / EEPROM ----------------------------------------

int bqBits(std::uint8_t reg, std::uint8_t mask, std::uint8_t value)
{
    std::uint8_t v = 0;
    esp_err_t e = i2cRead8(kBqAddr, reg, v);
    if (e == ESP_OK) {
        const auto nv = static_cast<std::uint8_t>((v & ~mask) | (value & mask));
        e = i2cWrite8(kBqAddr, reg, nv);
        std::printf("REG%02X 0x%02X -> 0x%02X\n", reg, v, nv);
    }
    return err("BQ25896", e);
}

int cmdBq(int argc, char** argv)
{
    i2cInit();
    long v = 0;
    if (argc == 1) {
        printCharger();
        return 0;
    }
    if (argc == 3 && is(argv[1], "ichg") && num(argv[2], v) && v >= 0
        && v <= 3008) {
        return bqBits(0x04, 0x7F, static_cast<std::uint8_t>(v / 64));
    }
    if (argc == 3 && is(argv[1], "iinlim") && num(argv[2], v) && v >= 100) {
        return bqBits(0x00, 0x3F, static_cast<std::uint8_t>((v - 100) / 50));
    }
    if (argc == 3 && is(argv[1], "wd")) {
        const std::uint8_t code = is(argv[2], "off") ? 0 : is(argv[2], "40") ? 1
                                : is(argv[2], "80")  ? 2 : 3;
        return bqBits(0x07, 0x30, static_cast<std::uint8_t>(code << 4));
    }
    if (argc == 3 && is(argv[1], "charge")) {
        return bqBits(0x03, 0x10, is(argv[2], "on") ? 0x10 : 0x00);
    }
    if (argc == 3 && is(argv[1], "hiz")) {
        return bqBits(0x00, 0x80, is(argv[2], "on") ? 0x80 : 0x00);
    }
    if (argc == 2 && is(argv[1], "adc")) {
        return bqBits(0x02, 0x40, 0x40); // continuous conversion, 1 s
    }
    return usage("bq | bq ichg <mA> | bq iinlim <mA> | bq wd off|40|80|160 |"
                 " bq charge on|off | bq hiz on|off | bq adc");
}

int cmdFg(int argc, char** argv)
{
    i2cInit();
    if (argc == 2 && is(argv[1], "clear")) {
        std::uint16_t cfg = 0;
        esp_err_t e = i2cRead16(kFgAddr, 0x0C, cfg);
        if (e == ESP_OK) {
            e = i2cWrite16(kFgAddr, 0x0C, static_cast<std::uint16_t>(cfg & ~0x20));
        }
        return err("ALRT azzerato", e);
    }
    if (argc == 2 && is(argv[1], "qstart")) {
        return err("quick start", i2cWrite16(kFgAddr, 0x06, 0x4000));
    }
    if (argc != 1) {
        return usage("fg | fg clear | fg qstart");
    }
    printGauge();
    return 0;
}

int cmdEe(int argc, char** argv)
{
    i2cInit();
    long addr = 0, n = 16;
    if (argc >= 2 && is(argv[1], "eui")) {
        std::array<std::uint8_t, 8> d = {};
        const esp_err_t e = i2cRead(kEeAddr, 0xF8, d.data(), d.size());
        hexdump(0xF8, d.data(), d.size());
        return err("EUI-64", e);
    }
    if (argc < 3 || !num(argv[2], addr) || addr < 0 || addr > 0xFF) {
        return usage("ee eui | ee r <addr> [n] | ee w <addr> <byte>... (solo 0x00-0x7F)");
    }
    if (is(argv[1], "r")) {
        if (argc > 3 && (!num(argv[3], n) || n < 1 || addr + n > 256)) {
            return usage("ee r <addr> [n]");
        }
        std::array<std::uint8_t, 256> d = {};
        const esp_err_t e = i2cRead(kEeAddr, static_cast<std::uint8_t>(addr),
                                    d.data(), static_cast<std::size_t>(n));
        if (e == ESP_OK) {
            hexdump(static_cast<std::uint32_t>(addr), d.data(),
                    static_cast<std::size_t>(n));
        }
        return err("lettura", e);
    }
    if (is(argv[1], "w") && argc >= 4) {
        // 16 byte pages: one byte per write keeps page limits out of the way.
        for (int i = 3; i < argc; ++i, ++addr) {
            long b = 0;
            if (!num(argv[i], b) || addr > 0x7F) {
                return usage("ee w <addr> <byte>... (solo 0x00-0x7F)");
            }
            const esp_err_t e = i2cWrite8(kEeAddr, static_cast<std::uint8_t>(addr),
                                          static_cast<std::uint8_t>(b));
            vTaskDelay(pdMS_TO_TICKS(6));
            if (e != ESP_OK) {
                return err("scrittura", e);
            }
        }
        std::printf("scritto\n");
        return 0;
    }
    return usage("ee eui | ee r <addr> [n] | ee w <addr> <byte>...");
}

// ---- ADAU1452 -----------------------------------------------------------

int adauRw(int argc, char** argv)
{
    long addr = 0, v = 0, n = 1;
    if (argc < 3 || !num(argv[2], addr) || addr < 0 || addr > 0xFFFF) {
        return usage("adau r <addr> [parole] | adau w <addr> <valore>");
    }
    const auto a = static_cast<std::uint16_t>(addr);
    const std::size_t wb = adauWordBytes(a);
    if (is(argv[1], "r")) {
        if (argc > 3 && (!num(argv[3], n) || n < 1 || n > 64)) {
            return usage("adau r <addr> [1..64]");
        }
        std::array<std::uint8_t, 256> d = {};
        const esp_err_t e = adauRead(a, d.data(), wb * static_cast<std::size_t>(n));
        for (long i = 0; e == ESP_OK && i < n; ++i) {
            const std::uint8_t* p = d.data() + i * wb;
            if (wb == 2) {
                std::printf("0x%04lX: 0x%04X\n", addr + i, (p[0] << 8) | p[1]);
            } else {
                const std::uint32_t w = (static_cast<std::uint32_t>(p[0]) << 24)
                                        | (p[1] << 16) | (p[2] << 8) | p[3];
                std::printf("0x%04lX: 0x%08lX (%.6f in 8.24)\n", addr + i,
                            static_cast<unsigned long>(w),
                            static_cast<std::int32_t>(w) / 16777216.0);
            }
        }
        return err("lettura", e);
    }
    if (is(argv[1], "w") && argc == 4 && num(argv[3], v)) {
        std::array<std::uint8_t, 4> d = {};
        const auto u = static_cast<std::uint32_t>(v);
        if (wb == 2) {
            d = {static_cast<std::uint8_t>(u >> 8), static_cast<std::uint8_t>(u)};
        } else {
            d = {static_cast<std::uint8_t>(u >> 24),
                 static_cast<std::uint8_t>(u >> 16),
                 static_cast<std::uint8_t>(u >> 8), static_cast<std::uint8_t>(u)};
        }
        return err("scrittura", adauWrite(a, d.data(), wb));
    }
    return usage("adau r <addr> [parole] | adau w <addr> <valore>");
}

int cmdAdau(int argc, char** argv)
{
    if (!needAnalog()) {
        return 1;
    }
    if (argc == 1 || is(argv[1], "status")) {
        printDspStatus();
        return 0;
    }
    if (is(argv[1], "reset")) {
        const bool boot = !(argc > 2 && is(argv[2], "noboot"));
        adauReset(boot);
        adauToggleCs3();
        std::printf("reset con SELFBOOT %s, SS commutato 3 volte\n",
                    boot ? "alto" : "basso");
        if (boot) {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        printDspStatus();
        return 0;
    }
    if (is(argv[1], "toggle")) {
        adauToggleCs3();
        std::printf("SS commutato 3 volte\n");
        return 0;
    }
    return adauRw(argc, argv);
}

// ---- Si4684 -------------------------------------------------------------

void printSiReply(const std::uint8_t* r, std::size_t n)
{
    std::printf("STATUS0 0x%02X (CTS=%u ERR=%u) STATUS3 0x%02X (PUP_STATE=%u)\n",
                r[0], r[0] >> 7, (r[0] >> 6) & 1, r[3], r[3] >> 6);
    if (n > 4) {
        hexdump(4, r + 4, n - 4);
    }
}

int siPowerUp(int argc, char** argv)
{
    long ibias = 72, ctun = 0, xtal = 19200000;
    if ((argc > 2 && !num(argv[2], ibias)) || (argc > 3 && !num(argv[3], ctun))
        || (argc > 4 && !num(argv[4], xtal))) {
        return usage("si pup [ibias] [ctun] [xtal_hz]");
    }
    const auto x = static_cast<std::uint32_t>(xtal);
    const std::array<std::uint8_t, 16> pup = {
        0x01, 0x00, 0x17, static_cast<std::uint8_t>(ibias),
        static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(x >> 8),
        static_cast<std::uint8_t>(x >> 16), static_cast<std::uint8_t>(x >> 24),
        static_cast<std::uint8_t>(ctun), 0x10, 0, 0, 0, 0x18, 0, 0};
    std::array<std::uint8_t, 4> r = {};
    const esp_err_t e = siCommand(pup.data(), pup.size(), r.data(), r.size());
    printSiReply(r.data(), r.size());
    return err("POWER_UP", e);
}

int cmdSi(int argc, char** argv)
{
    if (!needAnalog()) {
        return 1;
    }
    std::array<std::uint8_t, 60> r = {};
    if (argc >= 2 && is(argv[1], "reset")) {
        siReset();
        std::printf("RSTB# impulsato\n");
        return 0;
    }
    if (argc >= 2 && is(argv[1], "pup")) {
        return siPowerUp(argc, argv);
    }
    if (argc >= 2 && is(argv[1], "part")) {
        const std::array<std::uint8_t, 2> c = {0x08, 0x00};
        const esp_err_t e = siCommand(c.data(), c.size(), r.data(), 23);
        printSiReply(r.data(), 23);
        std::printf("PART %u\n", r[8] | (r[9] << 8));
        return err("GET_PART_INFO", e);
    }
    if (argc >= 4 && is(argv[1], "cmd")) {
        long len = 0;
        std::array<std::uint8_t, 60> c = {};
        std::size_t k = 0;
        if (!num(argv[2], len) || len < 4 || len > 59) {
            return usage("si cmd <byte_risposta 4..59> <byte>...");
        }
        for (int i = 3; i < argc && k < c.size(); ++i) {
            long b = 0;
            if (!num(argv[i], b)) {
                return usage("si cmd <byte_risposta> <byte>...");
            }
            c[k++] = static_cast<std::uint8_t>(b);
        }
        const esp_err_t e = siCommand(c.data(), k, r.data(),
                                      static_cast<std::size_t>(len));
        printSiReply(r.data(), static_cast<std::size_t>(len));
        return err("comando", e);
    }
    if (argc == 1 || is(argv[1], "status")) {
        const esp_err_t e = siReadReply(r.data(), 4);
        printSiReply(r.data(), 4);
        std::printf("INTB#=%d\n",
                    gpio_get_level(static_cast<gpio_num_t>(pins::Si4684Intb)));
        return err("RD_REPLY", e);
    }
    return usage("si status | si reset | si pup [ibias ctun xtal] | si part |"
                 " si cmd <n_risposta> <byte>...");
}

// ---- BT1058 -------------------------------------------------------------

int btListen(int seconds)
{
    std::array<char, 128> buf = {};
    const int64_t end = esp_timer_get_time() + seconds * 1000000LL;
    while (esp_timer_get_time() < end) {
        const int n = btRead(buf.data(), buf.size(), 100);
        if (n > 0) {
            std::printf("%s", printable(buf.data(), static_cast<std::size_t>(n))
                                  .c_str());
            std::fflush(stdout);
        }
    }
    std::printf("\n");
    return 0;
}

int cmdBt(int argc, char** argv)
{
    if (argc >= 2 && is(argv[1], "off")) {
        btPowerDown();
        std::printf("BT1058: RESET# e SYS_CTRL bassi\n");
        return 0;
    }
    if (!needAnalog()) {
        return 1;
    }
    long v = 0;
    if (argc >= 2 && is(argv[1], "on")) {
        std::printf("sequenza di accensione (2.7 s), poi ascolto 25 s...\n");
        if (!btPowerUp()) {
            std::printf("UART non inizializzata\n");
            return 1;
        }
        return btListen(25);
    }
    if (argc == 3 && is(argv[1], "listen") && num(argv[2], v)) {
        return btListen(static_cast<int>(v));
    }
    if (argc == 3 && is(argv[1], "baud") && num(argv[2], v)) {
        return btUartInit(static_cast<int>(v)) ? 0 : 1;
    }
    if (argc >= 3 && is(argv[1], "at")) {
        std::string line = argv[2];
        for (int i = 3; i < argc; ++i) {
            line += ' ';
            line += argv[i];
        }
        const std::string r = btAt(line.c_str(), 3000);
        std::printf("%s\n", printable(r.data(), r.size()).c_str());
        return r.empty() ? 1 : 0;
    }
    return usage("bt on | bt off | bt at <comando> | bt listen <s> | bt baud <n>");
}

// ---- I2S / alert / flash / reboot ---------------------------------------

int cmdI2s(int argc, char** argv)
{
    if (!needAnalog()) {
        return 1;
    }
    long a = 0, b = 0;
    if (argc == 1 || is(argv[1], "clocks")) {
        for (const int pin : {pins::I2sBclk, pins::I2sWs, pins::I2sDin,
                              pins::I2sDout}) {
            long edges = 0;
            double hz = 0;
            measureEdges(pin, 100, edges, hz);
            std::printf("GPIO%d: %.0f Hz (%ld fronti in 100 ms)\n", pin, hz,
                        edges);
        }
        return 0;
    }
    if (is(argv[1], "tone")) {
        a = 1000;
        b = 5;
        if ((argc > 2 && !num(argv[2], a)) || (argc > 3 && !num(argv[3], b))) {
            return usage("i2s tone [hz] [secondi]");
        }
        std::printf("tono %ld Hz per %ld s su GPIO%d (ingresso 3 del mux)\n", a,
                    b, pins::I2sDout);
        return err("tono", i2sTone(static_cast<int>(a), static_cast<int>(b)));
    }
    if (is(argv[1], "peak")) {
        a = 500;
        if (argc > 2 && !num(argv[2], a)) {
            return usage("i2s peak [ms]");
        }
        long l = 0, r = 0, frames = 0;
        const esp_err_t e = i2sPeak(static_cast<int>(a), l, r, frames);
        std::printf("%ld frame, picco L %ld R %ld su 8388607\n", frames, l, r);
        return err("lettura", e);
    }
    return usage("i2s clocks | i2s tone [hz] [s] | i2s peak [ms]");
}

int cmdAlert(int argc, char** argv)
{
    alertArm();
    if (argc > 1 && is(argv[1], "clear")) {
        alertClearCount();
    }
    std::printf("PWR_ALERT livello %d, %d fronti di discesa dall'ultimo clear\n",
                alertLevel(), alertEdges());
    return 0;
}

int cmdFlash(int /*argc*/, char** /*argv*/)
{
    std::uint8_t id[3] = {};
    const esp_err_t e = extFlashJedec(id);
    std::printf("U3 JEDEC ID %02X %02X %02X\n", id[0], id[1], id[2]);
    return err("lettura", e);
}

int cmdWifi(int /*argc*/, char** /*argv*/)
{
    std::vector<WifiAp> aps;
    const esp_err_t e = wifiScan(aps);
    for (const WifiAp& ap : aps) {
        std::printf("%-32s %4d dBm  canale %d\n", ap.ssid, ap.rssi, ap.channel);
    }
    return err("scansione Wi-Fi", e);
}

int cmdBle(int argc, char** argv)
{
    long s = 5;
    if (argc > 1 && (!num(argv[1], s) || s < 1 || s > 60)) {
        return usage("ble [secondi]");
    }
    int devices = 0, adverts = 0, rssi = 0;
    const esp_err_t e = bleScan(static_cast<int>(s), devices, adverts, rssi);
    std::printf("%d dispositivi, %d pacchetti, il piu' forte %d dBm\n", devices,
                adverts, rssi);
    return err("scansione BLE", e);
}

int cmdReboot(int /*argc*/, char** /*argv*/)
{
    analogOff();
    esp_restart();
    return 0;
}

void add(const char* name, const char* help, esp_console_cmd_func_t fn)
{
    esp_console_cmd_t c = {};
    c.command = name;
    c.help = help;
    c.func = fn;
    ESP_ERROR_CHECK(esp_console_cmd_register(&c));
}

} // namespace

void registerTestCommands()
{
    esp_console_register_help_command();
    add("run", "Rilancia il test hardware. 'run nobt' salta il BT1058", cmdRun);
}

void registerMonitorCommands()
{
    registerTestCommands();
    add("power", "power [on|off]: 3V3_AUDIO (ENABLE_ANALOG)", cmdPower);
    add("gpio", "gpio <n> [0|1|in|up|down|float]: legge o pilota un pin", cmdGpio);
    add("spi", "spi [hz <n>]: frequenza del bus SPI di controllo", cmdSpi);
    add("i2c", "i2c scan | r <addr> <reg> [n] | w <addr> <reg> <byte>...", cmdI2c);
    add("bq", "bq | ichg <mA> | iinlim <mA> | wd off|40|80|160 | charge on|off"
              " | hiz on|off | adc", cmdBq);
    add("fg", "fg | clear | qstart: MAX17048", cmdFg);
    add("ee", "ee eui | r <addr> [n] | w <addr> <byte>...: 24AA025E64", cmdEe);
    add("adau", "adau status | reset [noboot] | toggle | r <addr> [parole] |"
                " w <addr> <valore>", cmdAdau);
    add("si", "si status | reset | pup [ibias ctun xtal] | part |"
              " cmd <n_risposta> <byte>...", cmdSi);
    add("bt", "bt on | off | at <comando> | listen <s> | baud <n>", cmdBt);
    add("i2s", "i2s clocks | tone [hz] [s] | peak [ms]", cmdI2s);
    add("alert", "alert [clear]: livello e fronti su PWR_ALERT", cmdAlert);
    add("flash", "flash: JEDEC ID di U3", cmdFlash);
    add("wifi", "wifi: scansione delle reti", cmdWifi);
    add("ble", "ble [secondi]: scansione Bluetooth LE", cmdBle);
    add("reboot", "spegne 3V3_AUDIO e riavvia", cmdReboot);
}

void startConsole(const char* prompt)
{
    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    cfg.prompt = prompt;
    cfg.max_cmdline_length = 256;
    cfg.task_stack_size = 8192;
    ESP_ERROR_CHECK(esp_console_new_repl_stdio(&cfg, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

} // namespace bu
