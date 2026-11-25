# DigiRadio v2 bring-up tools

Two small firmwares for the first assembled v2 boards. They touch the chips
one register at a time and share no code with the radio firmware except the
pin map ([`../firmware/main/board_pins.hpp`](../firmware/main/board_pins.hpp)),
so a faulty block shows up as the bus operation that fails.

| Folder | What it does |
|---|---|
| [`hwtest`](hwtest) | Tests every block in turn at boot and prints a report with `[ OK ]`, `[WARN]`, `[FAIL]`, `[SKIP]` and a summary. `run` repeats it, `run nobt` skips the BT1058. |
| [`monitor`](monitor) | Interactive console: I2C, SPI, GPIO, power, DSP and tuner registers, BT1058 AT commands, I2S clocks and test tone. Starts with 3V3_AUDIO off. |
| [`components/v2bringup`](components/v2bringup) | The shared bus code, the self test and the console commands. |

Neither firmware has Wi-Fi, PSRAM or the Si4684 images: nothing proprietary
is in the binaries.

## Flash

Ready images, merged from offset 0 (bootloader, partition table, app), are in
[`bin/`](bin). Over the ESP32-S31 USB port:

```bash
python -m esptool --chip esp32s31 -b 460800 write-flash 0x0 bin/digiradio-v2-hwtest.bin
```

Then open the console at 115200 (any speed works on USB Serial/JTAG):

```bash
python -m serial.tools.miniterm /dev/cu.usbmodem* 115200
```

To build them yourself:

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
```

```bash
cd hwtest && idf.py --preview set-target esp32s31 && idf.py build
```

## What the hardware test checks

| Block | Check |
|---|---|
| I2C | Scan; 0x36, 0x50 and 0x6B must answer. |
| BQ25896 | Part number (REG14), I2C watchdog off (REG07) so the settings stay, charge current set to 256 mA (REG04), one ADC conversion: VBAT, VSYS, VBUS, charge current, TS, status and faults. |
| MAX17048 | VERSION, VCELL, SOC, CRATE, CONFIG, STATUS; VCELL against the BQ25896 VBAT. |
| 24AA025E64 | EUI-64 at 0xF8; write, read back and restore byte 0x7F. |
| PWR_ALERT GPIO7 | Idle high; with USB connected, charging off and on again must give a falling edge. |
| 3V3_AUDIO | ENABLE_ANALOG high with the audio pins floating, then SMODE low. |
| ADAU1452 | Reset with SELFBOOT released, three SS toggles, CORE_STATUS for up to 4 s, PLL_LOCK, PANIC_FLAG and PANIC_CODE. Without a self boot: reset with SELFBOOT low and write/read back PLL_CTRL0, to tell an SPI fault from an EEPROM fault. Runs before the Si4684, which the DSP would otherwise read as I2C traffic. |
| Si4684 | Reset, CTS and PUP_STATE 0; POWER_UP (19.2 MHz, IBIAS 72, CTUN 0); PUP_STATE 2; GET_PART_INFO must say 4684. No patch is loaded, so the W25Q16 (U7) is not tested here. |
| I2S | Only with the DSP running: WS near 48 kHz, 64 BCLK per frame, activity on DIN (GPIO42), ESP32 slave receiving frames. DOUT (GPIO48) is tested by ear with `i2s tone` in the monitor. |
| TOSLINK | SPDIF_TX_EN and SPDIF_TX_CTRL; the red light in J5 is checked by eye. |
| BT1058 | RESET# and SYS_CTRL low 2.5 s, RESET# high, SYS_CTRL low 200 ms then high; banner for up to 25 s; `AT`; then `AT+VER`, `AT+NAME`, `AT+MODCFG`, `AT+AUXCFG`, `AT+I2SCFG` replies are printed. With no reply at 115200, other baud rates are tried. |
| U3 W25Q128 | JEDEC ID (EF xx 18). |

The test leaves the BQ25896 watchdog off and the charge current at 256 mA
until the battery is disconnected. The radio firmware does not write them.

## Monitor commands

`help` lists them all. Numbers take decimal or 0x.

| Command | Use |
|---|---|
| `power on` / `power off` | 3V3_AUDIO. Most other commands need it on. |
| `gpio 19` / `gpio 19 1` / `gpio 7 up` / `gpio 7 float` | Read or drive any pin. |
| `i2c scan`, `i2c r 0x6b 0x00 0x15`, `i2c w 0x6b 0x04 0x04` | Raw I2C. |
| `bq`, `bq ichg 512`, `bq wd off`, `bq charge off`, `bq hiz on`, `bq adc` | BQ25896 decoded and its common settings. |
| `fg`, `fg clear`, `fg qstart` | MAX17048, clear ALRT, quick start. |
| `ee eui`, `ee r 0 32`, `ee w 0x10 1 2 3` | EEPROM; writes are refused above 0x7F. |
| `adau reset`, `adau reset noboot`, `adau status`, `adau r 0xf405`, `adau r 0 8`, `adau w 0xf000 0x60` | DSP. Registers (0xF000 up) are 16 bit, memory is 32 bit and printed in 8.24 too. |
| `si reset`, `si pup`, `si part`, `si status`, `si cmd 8 0x09 0x00` | Si4684 commands; `cmd` takes the reply length first. |
| `bt on`, `bt at AT+VER`, `bt listen 10`, `bt baud 9600`, `bt off` | BT1058. |
| `i2s clocks`, `i2s tone 1000 5`, `i2s peak 500` | Clock frequencies with the PCNT, a tone on DOUT to mux input 3, peak level on DIN. |
| `alert`, `alert clear` | PWR_ALERT level and falling edges counted. |
| `spi hz 4000000` | Control SPI speed (default 1 MHz). |
| `flash` | U3 JEDEC ID. |
| `run`, `run nobt` | The whole hardware test. |
| `reboot` | 3V3_AUDIO off, then restart. |

## Known limits

- MAX17048 register meanings follow the map already used by
  `power_monitor.cpp`; its data sheet is not in the repo yet.
- The I2S checks assume the DSP program is master at 48 kHz with 32 bit slots,
  as the v2 firmware does.
- The BT1058 reply formats are printed raw: nothing is parsed.
