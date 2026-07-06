# ADAU1701 SigmaStudio export

SigmaStudio project **DigiRadio**, IC1 = ADAU1701.

## Key files

| File | Role |
|------|------|
| `DigiRadio_IC_1.h` | Program/param RAM data + `default_download_IC_1()` |
| `DigiRadio_IC_1_REG.h` | Control register map (safeload trigger) |
| `DigiRadio_IC_1_PARAM.h` | Parameter cell indices (`ADDR_*`) |
| `DigiRadio_NetList.xml` | Schematic netlist (reference) |
| `SigmaStudioFW.h` | Safeload / I2C glue API |

The ESP32 replays `default_download_IC_1()` over I2C on every boot via
`adau1701::Adau1701Driver`. Runtime mixer/EQ/master changes use the
ADAU1701 safeload mechanism (`sigma_safeload_param` /
`sigma_safeload_block` in `components/drivers/adau1701/src/SigmaStudioFW.c`).

I2C address: 7-bit `0x34` (ADDR0=ADDR1=GND), matching `board_pins.hpp`.

Sample rate: **48 kHz** (see `DigiRadio_NetList.xml`).

## After re-exporting from SigmaStudio

1. **Action → Export System Files** into this folder (overwrite headers).
2. Reconcile new `ADDR_*` symbols in `components/drivers/adau1701/include/adau1701/Adau1701ParamMap.hpp` if block names moved.
3. Rebuild and flash firmware.
4. Update the manual: `docs/manual/ch-sigmastudio.tex` (design) and
   `docs/manual/ch-adau1701.tex` (driver/API) if the signal chain changed.

## Documentation

- **Manual — SigmaStudio design:** `docs/manual/ch-sigmastudio.tex`
- **Manual — driver & usage:** `docs/manual/ch-adau1701.tex` (boot, safeload,
  `Adau1701Driver`, `AudioService`, HTTP `/api/audio/*`)
- **HTTP schemas:** `docs/manual/ch-api.tex`

Do **not** use SigmaStudio *Link Compile Download* on DigiRadio — export
only; the ESP32 programs the DSP at every power-up.
