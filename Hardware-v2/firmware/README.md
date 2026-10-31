# DigiRadio v2 firmware

Firmware for the second DigiRadio board ([`../kicad`](../kicad)). Everything
without a board difference is shared with the v1 firmware in
[`../../Software`](../../Software): tuner, Bluetooth and audio services, web
server, web radio, alarms, OTA. This folder holds only what differs.

**Status:** it compiles; it has never run. There is no v2 hardware yet, so
nothing below has been tested on a board.

## Build

ESP-IDF 6.1 is needed, because the ESP32-S31 is a preview target there. The v1
firmware stays on its own IDF.

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
```

```bash
idf.py --preview set-target esp32s31
```

```bash
idf.py build
```

The Si4684 images must be in `../../Software/Firmware/Si4684-Firmware`. They are proprietary and not in this repo: see the README in that folder. The ADAU1452 program is in [`dsp/`](dsp/).

## What differs from v1

| | v1 | v2 |
|---|---|---|
| Module | ESP32-S3-WROOM-1 | ESP32-S31-WROOM-3-N16R16V (RISC-V, 16 MB flash, 16 MB PSRAM) |
| DSP | ADAU1701 on I2C, program sent at each boot | ADAU1452 on SPI, self boot from 25AA1024, SPI download if that fails |
| Bluetooth | FSC-BT1035 | FSC-BT1058, UART without CTS/RTS, I2S 24 bit |
| Tuner images | Sent by the ESP32 at each boot | Stored once in the W25Q16 behind the Si4684, then `FLASH_LOAD` |
| Identity EEPROM | 24AA025E48 at 0x52 | 24AA025E64 at 0x50 |
| Power | Always on | 3V3_AUDIO switched by ENABLE_ANALOG; MAX17048 and BQ25896 on I2C |
| Audio outputs | Bluetooth | Bluetooth and TOSLINK (S/PDIF comes straight from the DSP program) |

The pin map is in [`main/board_pins.hpp`](main/board_pins.hpp), taken from the
netlist.

## Boot order

1. ENABLE_ANALOG goes high with every audio pin floating, then SMODE goes low.
2. ADAU1452: reset with SELFBOOT released, three dummy writes to enter SPI mode, then check CORE_STATUS and PANIC_FLAG. This comes first because it would otherwise read Si4684 traffic as I2C. Without a self boot, a second reset with SELFBOOT low (GPIO22) and an SPI download.
3. If the EEPROM image differs from the one in the app, write and verify it, then reboot the DSP.
4. Si4684: load the patch, check both images with `FLASH_CHECK_CRC32`, rewrite them if different.
5. Si4684 boots with `FLASH_LOAD`. If the flash step failed, it uses `HOST_LOAD` as on v1.
6. BT1058 starts in the background, as on v1.

A DSP failure does not stop the boot, so the web server and OTA stay reachable.

## Updating the DSP without reflashing

| Request | Effect at next boot |
|---|---|
| `POST /api/dsp/program`, DRAD blob from `tools/sigma_export.py` | Runs this program over SPI with SELFBOOT low, ignoring the EEPROM. To try a program, or to recover |
| `DELETE /api/dsp/program` | Back to the EEPROM, or to the program in the app |
| `POST /api/dsp/eeprom`, raw self boot image | Writes it to the 25AA1024 with the writer program and verifies it, then boots from it. Takes minutes: keep the power on |

Each request reboots the radio. Uploads stay in the `dsp` partition. An image already written is not written again even if it fails to boot; upload another one.

## To check at bring-up

These were written from documents or by analogy, and only a board can confirm them.

- **BT1058.** The driver follows the [AT Command Set](https://document.feasycom.com/docs/audio/BT1058_EN/latest/at-cmd-set.html): source mode, `AT+AUXCFG=3`, `AT+I2SCFG=35`, one reboot when a setting changed. Still open: that 24 bit data works with the DSP's 64 BCLK per frame (32 bit slots), and the exact reply format of `AT+MODCFG`. If that format differs, the module reboots at every boot. The scan and pairing replies come from the BT1035 parser. Feasycom has not answered about I2S slave clocking.
- **CRC of `FLASH_CHECK_CRC32`.** AN649 does not name the polynomial; the code uses CRC-32 IEEE. A wrong guess would rewrite the flash at every boot, and the log would show it.
- **Self boot timing.** `waitForSelfBoot()` reads the status during the attempt. If that misbehaves, the timeout still ends in a reset with SELFBOOT low and an SPI download.
- **MAX17048 and BQ25896 registers.** Their data sheets are not in the repo.
- **Si4684 crystal.** Run `tools/si4684_xtal_calibration.py`. The defaults are nominal frequency and CTUN 0, not the v1 trim.
- **PMIC_INT and MAX17048_ALERT.** One open-drain line to GPIO7, per Michele's schematic change. The firmware polls as well, so a late edge only delays a reading.

## Not done

- **Self boot image.** SigmaStudio+ makes it only while writing to a board over USBi. On the first board: wire the USBi to the ADAU1452 SPI, write from SigmaStudio+, read the EEPROM back, and save that as `dsp/adau1452_selfboot.bin`. From then on the firmware writes it to other boards with the writer program. Until then every boot downloads the program over SPI, which is normal and not an error. To read it back, enable `CONFIG_DIGIRADIO_DUMP_DSP_EEPROM`, save the serial log and run `tools/eeprom_log_to_bin.py`.
- **Published builds without Si4684 images.** The app must carry them, since it fills U7 and sends the patch.
- **SigmaStudio TCP bridge.** It is ADAU1701 only and does nothing on v2.
- **DSP to ESP32 audio (GPIO42) and the second flash (U3).** Wired, but unused.
