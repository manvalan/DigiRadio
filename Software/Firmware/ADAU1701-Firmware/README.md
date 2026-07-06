# ADAU1701 SigmaStudio export

SigmaStudio project **DigiRadio**, IC1 = ADAU1701.

Key files:
- `DigiRadio_IC_1.h` — program/param RAM data + `default_download_IC_1()`
- `DigiRadio_IC_1_REG.h` — register map
- `DigiRadio_IC_1_PARAM.h` — parameter handles (safeload addresses)

The ESP32 replays `default_download_IC_1()` over I2C on every boot via
`adau1701::Adau1701Driver`. Runtime mixer/EQ/master changes use the
ADAU1701 safeload mechanism (`sigma_safeload_param` /
`sigma_safeload_block` in `SigmaStudioFW.c`).

I2C address: 7-bit `0x34` (ADDR0=ADDR1=GND), matching `board_pins.hpp`.

Sample rate: **48 kHz** (see `DigiRadio_NetList.xml`).
