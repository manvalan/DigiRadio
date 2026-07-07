# DigiRadio Firmware

Open-source Hi-Fi DAB+/FM receiver firmware for the ESP32-S3.

**Status:** fw **0.8.3** on `main` — NVS + flash encryption (development mode),
tabbed Web UI, `IntegrationService`, RDS/DLS metadata, **13** host tests, **4** CI
jobs. Agent tasks T1–T8 complete; device HIL pending first PCB.

| Area | Shipped in 0.8.3 |
|------|------------------|
| Boot | Si4684 HOST_LOAD, ADAU1701 RAM program, BT1035 Line-In init |
| Tuner | FM/DAB tune, seek, RSQ, RDS, DAB services + DLS |
| Audio | 6-band EQ, mixer, stereo/bass enhance, NVS profile |
| Presets | CRUD, reorder, integrated recall + last-preset at boot |
| Network | SoftAP/STA, tabbed gzipped SPA, typed JSON REST API |
| Security | `initEncryptedStorage()` — see [`docs/security-flash-nvs.md`](docs/security-flash-nvs.md) |
| Quality | Doxygen, manual sync, Si4684 blob policy check |

Architecture: **functional core** (`components/core`, host-tested, no ESP-IDF) +
**imperative shell** (drivers, services, net, secure_store). Rules:
[`AGENTS.md`](AGENTS.md) · backlog: [`docs/TODO.md`](docs/TODO.md)

## Prerequisites

**Si4684 blobs** (proprietary, local only — not in git):

```bash
python3 tools/fetch_si4684_firmware.py --dab-only
python3 tools/fetch_si4684_firmware.py --si46xx-dir /path/to/si46xx_firmware
python3 tools/check_si4684_blobs.py
```

See [`Firmware/Si4684-Firmware/README.md`](Firmware/Si4684-Firmware/README.md).

**ESP-IDF** v5.5.x, target `esp32s3`.

## Quick start

Open this directory (`Software/`) as the Cursor project so `AGENTS.md` and
`.cursor/rules/` load automatically.

```bash
idf.py set-target esp32s3
idf.py build
idf.py erase-flash flash   # once when first enabling encryption (0.8.3+)
idf.py -p <port> monitor
```

Production flash-encryption release mode: overlay `sdkconfig.defaults.production`
(see security doc — irreversible on chip).

## Host tests

Pure core, no hardware (needs C++23 compiler on macOS — Homebrew `llvm` or GCC 14):

```bash
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## Quality gates (CI on every push to `main`)

Run from `Software/`:

```bash
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
python3 tools/gzip-www.sh   # after editing components/net/www/index.html
```

CI jobs: `host-tests`, `doxygen`, `manual-sync`, `si4684-blobs`
(`.github/workflows/ci.yml`).

## Web UI

Gzipped single-page app at `/` — tabs: **Now** (RDS/DLS), **Radio**, **Presets**,
**Audio** (6-band EQ), **BT**, **Wi‑Fi**. Source:
`components/net/www/index.html` · regenerate embed:
`tools/gzip-www.sh`.

## HTTP API (fw 0.8.3)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/health` | Status, firmware version, companion-chip flags |
| POST | `/api/wifi` | Provision STA credentials; reboot on success |
| GET | `/api/tuner/status` | Tuner snapshot (DAB/FM, RDS/DLS metadata) |
| GET | `/api/tuner/services` | DAB service list for current ensemble |
| POST | `/api/tuner/tune` | Tune DAB ensemble or FM frequency |
| POST | `/api/tuner/play` | Start DAB service playback |
| POST | `/api/tuner/seek` | FM seek up |
| GET/PUT | `/api/audio/profile` | Read/apply ADAU1701 mixer + 6-band EQ |
| POST | `/api/audio/reset` | Factory-flat audio profile |
| POST | `/api/audio/stereo-enhance` | Stereo depth overlay (0–100) |
| POST | `/api/audio/bass-enhance` | Bass enhance overlay (0–100) |
| GET | `/api/bluetooth/status` | BT1035 boot, pairing, A2DP state |
| POST | `/api/bluetooth/pair` | Enter discoverable mode |
| POST | `/api/bluetooth/pair/stop` | Leave discoverable mode |
| POST | `/api/bluetooth/disconnect` | Release A2DP session |
| GET | `/api/stations` | List saved presets |
| POST | `/api/stations` | Add preset |
| POST | `/api/stations/remove` | Remove preset by index |
| POST | `/api/stations/reorder` | Move preset (`from`/`to` indices) |
| POST | `/api/stations/tune` | Recall preset (tuner + audio profile + NVS) |

Wire schemas: [`docs/manual/ch-api.tex`](docs/manual/ch-api.tex) · C++ API:
`doxygen Doxyfile` → `docs/api/html/index.html`.

## Layout

| Path | Role |
|------|------|
| `Firmware/` | Si4684 `.bin` (local) + ADAU1701 SigmaStudio export |
| `components/core/` | Pure domain (host-tested) |
| `components/drivers/` | Si4684, ADAU1701, BT1035 |
| `components/services/` | Tuner, audio, Bluetooth, station, integration |
| `components/secure_store/` | Encrypted NVS: credentials, profiles, presets |
| `components/net/` | Wi-Fi, HTTP server, gzipped web UI |
| `sdkconfig.defaults` | C++23, NVS + flash encryption (dev mode) |
| `partitions.csv` | `nvs` + `nvs_keys` partitions |
| `docs/manual/` | LaTeX technical manual |
| `docs/security-flash-nvs.md` | Encryption + device HIL checklist |
| `docs/TODO.md` | Completed tasks + HIL backlog |
| `tools/` | Blob fetch, CI policy checks, UI gzip |

Manual PDF: `cd docs/manual && latexmk -lualatex manual.tex`

## Licence

Apache-2.0 — see [`LICENSE`](LICENSE).
