# Si4684 firmware images (local only — not in git)

Skyworks Si4684 application firmware is **proprietary**. DigiRadio fw **0.8.3**
embeds these blobs via ESP-IDF `EMBED_FILES`; they must **never** be committed.
CI runs `tools/check_si4684_blobs.py` on every push to `main`.

| File | Size (typ.) | Role |
|------|-------------|------|
| `rom_patch_016.bin` | 5796 B | ROM patch / bootloader helper (HOST_LOAD before main image) |
| `dab_firmware.bin` | ~517 KB | DAB+ application image (PE5PVB / Skyworks BIF) |
| `fm_firmware.bin` | ~530 KB | FM application image (Skyworks `fm_radio_5_1_0.bin`, AN649 A10) |

Boot sequence (AN649): POWER_UP → LOAD_INIT → HOST_LOAD(patch) → LOAD_INIT →
HOST_LOAD(image) → BOOT. DAB and FM share the same patch; only the application
image differs.

## Legal / redistribution

- **Do not** commit `*.bin` files here, attach them to GitHub releases, or
  redistribute them with this open-source tree.
- DAB images extracted from [PE5PVB/SI4684-DAB-Receiver](https://github.com/PE5PVB/SI4684-DAB-Receiver) are community-sourced; FM images come from the
  Skyworks evaluation package or equivalent local sources (see below).
- Keep blobs on your machine only. CI verifies they are gitignored and absent
  from history (`python3 tools/check_si4684_blobs.py`).

## Before you build

From `Software/`:

```bash
python3 tools/fetch_si4684_firmware.py --dab-only          # patch + DAB
python3 tools/fetch_si4684_firmware.py --si46xx-dir /path  # add FM (see below)
ls -la Firmware/Si4684-Firmware/*.bin                      # expect 3 files
```

`idf.py build` fails at link/embed time if blobs are missing.

## Obtain images locally

### Patch + DAB (automated, network)

```bash
cd Software
python3 tools/fetch_si4684_firmware.py --dab-only
```

Clones PE5PVB/SI4684-DAB-Receiver and writes `rom_patch_016.bin` and
`dab_firmware.bin`.

### FM image (manual source — pick one)

FM is **not** shipped with PE5PVB. Supply one of:

| Source | Command |
|--------|---------|
| Skyworks eval / dabpi `si46xx_firmware/` | `python3 tools/fetch_si4684_firmware.py --si46xx-dir /path/to/si46xx_firmware` |
| uGreen DABBoard `Files_v16.zip` | `python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli ~/Downloads/Files_v16.zip` |
| Full SPI flash dump (TechniSat layout) | `python3 tools/fetch_si4684_firmware.py --flash-dump technisat_spi.bin` |

Candidate FM filenames (AN649 / community): `fm_radio_5_1_0.bin`,
`fm_radio_5_0_9.bin`, `fmhd_radio_5_1_0.bin`, `fmhd_radio_5_0_4.bin`.

Manual copy helper:

```bash
python3 tools/extract_si4684_blob.py --copy /path/to/fm_radio_5_1_0.bin \
    --out Firmware/Si4684-Firmware/fm_firmware.bin
```

### All three from uGreen radio_cli

```bash
python3 tools/fetch_si4684_firmware.py --from-ugreen-radio-cli ~/Downloads/Files_v16.zip --ugreen-all
```

## Verify your checkout

```bash
cd Software
python3 tools/check_si4684_blobs.py    # must exit 0
git ls-files 'Firmware/Si4684-Firmware/*.bin'   # must print nothing
```

## References

- Skyworks **AN649** — Si4684 programming API and boot flow
- `tools/fetch_si4684_firmware.py` — fetch/extract orchestration
- `tools/extract_si4684_blob.py` — header/array → raw `.bin`
- `docs/manual/ch-si4684.tex` — driver boot integration in DigiRadio
