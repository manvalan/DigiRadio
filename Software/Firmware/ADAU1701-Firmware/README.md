# ADAU1701 SigmaStudio export

SigmaStudio project **DigiRadio**, IC1 = ADAU1701.

Key files:
- `DigiRadio_IC_1.h` — program/param RAM data + `default_download_IC_1()`
- `DigiRadio_IC_1_REG.h` — register map
- `DigiRadio_IC_1_PARAM.h` — parameter handles (safeload, Slice 5+)

The ESP32 replays `default_download_IC_1()` over I2C on every boot via
`adau1701::Adau1701Driver` (see `components/drivers/adau1701/`).

I2C address: 7-bit `0x34` (ADDR0=ADDR1=GND), matching `board_pins.hpp`.
