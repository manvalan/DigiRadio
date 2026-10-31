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
| Bluetooth | FSC-BT1035 | FSC-BT1058, UART without CTS/RTS |
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

## To check at bring-up

These were written from documents or by analogy, and only a board can confirm them.

- **BT1058 AT commands.** The driver is the BT1035 one, and no BT1058 programming guide is in the repo. `AT+AUXCFG` and `AT+I2SCFG=67` in particular may not apply. Feasycom has not yet answered about I2S slave mode, sample rate and BCLK ratio.
- **CRC of `FLASH_CHECK_CRC32`.** AN649 does not name the polynomial; the code uses CRC-32 IEEE. A wrong guess would rewrite the flash at every boot, and the log would show it.
- **Self boot timing.** `waitForSelfBoot()` reads the status during the attempt. If that misbehaves, the timeout still ends in a reset with SELFBOOT low and an SPI download.
- **MAX17048 and BQ25896 registers.** Their data sheets are not in the repo.
- **Si4684 crystal.** Run `tools/si4684_xtal_calibration.py`. The defaults are nominal frequency and CTUN 0, not the v1 trim.
- **PMIC_INT and MAX17048_ALERT.** One open-drain line to GPIO7, per Michele's schematic change. The firmware polls as well, so a late edge only delays a reading.

## Not done

- **Self boot image.** SigmaStudio+ makes it only while writing to a board over USBi. On the first board: wire the USBi to the ADAU1452 SPI, write from SigmaStudio+, read the EEPROM back, and save that as `dsp/adau1452_selfboot.bin`. From then on the firmware writes it to other boards with the writer program. Until then every boot downloads the program over SPI, which is normal and not an error. To read it back, enable `CONFIG_DIGIRADIO_DUMP_DSP_EEPROM`, save the serial log and run `tools/eeprom_log_to_bin.py`.
- **Published builds without Si4684 images.** The app must carry them, since it fills U7 and sends the patch.
- **SigmaStudio TCP bridge and `POST /api/dsp/program`.** Both are ADAU1701 only and do nothing on v2.
- **DSP to ESP32 audio (GPIO42) and the second flash (U3).** Wired, but unused.
