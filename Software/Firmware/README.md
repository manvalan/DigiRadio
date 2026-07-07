# DigiRadio — companion-chip firmware assets

Binary and SigmaStudio exports loaded by the ESP32 at every boot (fw **0.8.3**).

| Directory | Chip | In git? | Loaded by |
|-----------|------|---------|-----------|
| `ADAU1701-Firmware/` | ADAU1701 SigmaDSP | Yes (headers) | `Adau1701Driver` — RAM program every boot |
| `Si4684-Firmware/` | Si4684 DAB+/FM | README only | `Si4684Driver` — HOST_LOAD embedded blobs |

## Before first device build

Si4684 application images are **proprietary** and **gitignored**. Populate locally:

```bash
cd Software
python3 tools/fetch_si4684_firmware.py --dab-only
python3 tools/fetch_si4684_firmware.py --si46xx-dir /path/to/si46xx_firmware
python3 tools/check_si4684_blobs.py
ls Firmware/Si4684-Firmware/*.bin   # expect 3 files
```

Without blobs, `idf.py build` fails at embed/link time.

Full procurement and legal notes: [`Si4684-Firmware/README.md`](Si4684-Firmware/README.md).

## Si4684 blobs

| File | Role |
|------|------|
| `rom_patch_016.bin` | ROM patch (HOST_LOAD before main image) |
| `dab_firmware.bin` | DAB+ application (PE5PVB / Skyworks) |
| `fm_firmware.bin` | FM application (Skyworks eval or community source) |

Boot sequence: AN649 — POWER_UP → LOAD_INIT → HOST_LOAD(patch) → LOAD_INIT →
HOST_LOAD(image) → BOOT.

## ADAU1701 export

SigmaStudio project **DigiRadio** — exported headers replayed over I2C on every
boot (no self-boot EEPROM). Runtime EQ/mixer/enhancements via safeload and the
Web UI **Audio** tab (`/api/audio/*`).

Details: [`ADAU1701-Firmware/README.md`](ADAU1701-Firmware/README.md) · manual:
`docs/manual/ch-adau1701.tex`, `docs/manual/ch-sigmastudio.tex`.

## Security note

User settings (Wi-Fi, presets, audio profiles) live in **encrypted NVS**, not in
this folder. See `docs/security-flash-nvs.md`.
