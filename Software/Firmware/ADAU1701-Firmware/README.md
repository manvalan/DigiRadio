# ADAU1701 SigmaStudio export

SigmaStudio project **DigiRadio**, IC1 = ADAU1701. Shipped with DigiRadio firmware
**0.8.3**.

## Key files

| File | Role |
|------|------|
| `DigiRadio_IC_1.h` | Program/param RAM data + `default_download_IC_1()` |
| `DigiRadio_IC_1_REG.h` | Control register map (safeload trigger) |
| `DigiRadio_IC_1_PARAM.h` | Parameter cell indices (`ADDR_*`) |
| `DigiRadio_NetList.xml` | Schematic netlist (reference) |
| `SigmaStudioFW.h` | Safeload / I2C glue API |

## Runtime (fw 0.8.3)

- **Boot:** ESP32 replays `default_download_IC_1()` over I2C via
  `adau1701::Adau1701Driver` on every power-up.
- **Live control:** six-band PEQ, input mixer, master volume, stereo/bass
  enhancement overlays — safeload via `audio::AudioService` and REST
  `/api/audio/*`; profile persisted in encrypted NVS.
- **Web UI:** **Audio** tab — 6 EQ sliders + mixer/enhance controls.

I2C address: 7-bit `0x34` (ADDR0=ADDR1=GND). Sample rate: **48 kHz**.

Safeload implementation: `components/drivers/adau1701/src/SigmaStudioFW.c`.
Parameter map: `components/drivers/adau1701/include/adau1701/Adau1701ParamMap.hpp`.

## After re-exporting from SigmaStudio

1. **Action → Export System Files** into this folder (overwrite headers).
2. Reconcile new `ADDR_*` symbols in `Adau1701ParamMap.hpp` if block names moved.
3. Rebuild and flash firmware (`idf.py build`).
4. Update manual: `docs/manual/ch-sigmastudio.tex`, `docs/manual/ch-adau1701.tex`.

Do **not** use SigmaStudio *Link Compile Download* on DigiRadio — export only;
the ESP32 programs the DSP at every power-up.

## Documentation

- Design: `docs/manual/ch-sigmastudio.tex`
- Driver & API: `docs/manual/ch-adau1701.tex`
- HTTP: `docs/manual/ch-api.tex` (`/api/audio/*`)
