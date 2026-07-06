# Si4684 firmware images

| File | Size (typ.) | Role |
|------|-------------|------|
| `rom_patch_016.bin` | 5796 | ROM patch / bootloader helper (HOST_LOAD before main image) |
| `dab_firmware.bin` | ~517 KB | DAB+ application image (PE5PVB / Skyworks BIF) |
| `fm_firmware.bin` | ~530 KB | FM application image (Skyworks `fm_radio_5_1_0.bin`, AN649 A10) |

Boot sequence: AN649 — POWER_UP → LOAD_INIT → HOST_LOAD(patch) → LOAD_INIT →
HOST_LOAD(image) → BOOT. DAB and FM use the same patch; only the application
image differs.

## Refresh blobs

```bash
# DAB + patch from PE5PVB GitHub (always safe to re-run):
python3 tools/fetch_si4684_firmware.py --dab-only

# FM from Skyworks eval / dabpi si46xx_firmware folder:
python3 tools/fetch_si4684_firmware.py --si46xx-dir /path/to/si46xx_firmware

# FM extracted from uGreen DABBoard radio_cli (Files_v16.zip from ugreen.eu/downloads):
python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli ~/Downloads/Files_v16.zip

# Patch + DAB + FM entirely from uGreen radio_cli (overrides PE5PVB DAB):
python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli ~/Downloads/Files_v16.zip --ugreen-all

# FM extracted from a full SPI flash dump (TechniSat layout, dirb.me tech wiki):
python3 tools/fetch_si4684_firmware.py --flash-dump technisat_spi.bin
```

FM images are **not** redistributed with PE5PVB (DAB-only project). Obtain
`fm_radio_5_1_0.bin` from the Si4684 evaluation package (AN649 table 1),
community `si46xx_firmware/` folders used by [teknoid/dabpi](https://github.com/teknoid/dabpi),
or extract from a uGreen [DABBoard](https://ugreen.eu/downloads/) `radio_cli`
binary (proprietary — keep local, do not commit to public git).

Low-level conversion helper:

```bash
python3 tools/extract_si4684_blob.py --copy /path/to/fm_radio_5_1_0.bin \\
    --out Firmware/Si4684-Firmware/fm_firmware.bin
```
