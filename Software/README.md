# DigiRadio Firmware

Open-source Hi-Fi DAB+/FM receiver firmware for the ESP32-S3.

**Status:** fw **0.8.3** — NVS + flash encryption (dev mode); tabbed Web UI;
`IntegrationService`; **13** host tests; CI on `main`.
See [`docs/TODO.md`](docs/TODO.md) for the agent task list.

## Quick start

Open this directory (`Software/`) as the Cursor project so `AGENTS.md` and
`.cursor/rules/` load automatically.

```bash
idf.py set-target esp32s3
idf.py build
idf.py erase-flash flash   # once when upgrading to encrypted NVS (0.8.3+)
idf.py -p <port> flash monitor
```

Host unit tests (pure core, no hardware):

```bash
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Documentation gates (must exit 0 before merging; also enforced in CI):

```bash
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
python3 tools/gzip-www.sh   # after editing components/net/www/index.html
```

Manual PDF (design + HTTP API + class reference):

```bash
cd docs/manual && latexmk -lualatex manual.tex
```

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
| GET/PUT | `/api/audio/profile` | Read/apply ADAU1701 mixer + EQ profile |
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

Full schemas, error tokens, and boot flow: [`docs/manual/ch-api.tex`](docs/manual/ch-api.tex).
C++ signatures: `doxygen Doxyfile` → `docs/api/html/index.html`.

## Layout

| Path | Role |
|------|------|
| `Firmware/` | Si4684 `.bin` blobs (DAB+FM) + ADAU1701 SigmaStudio export |
| `components/core/` | Pure domain (host-tested) |
| `components/drivers/` | Si4684, ADAU1701, BT1035 drivers |
| `components/services/` | Tuner, audio, Bluetooth, station, integration services |
| `components/net/` | Wi-Fi, HTTP server, gzipped web UI |
| `docs/manual/` | LaTeX technical manual (canonical) |
| `docs/security-flash-nvs.md` | NVS + flash encryption and HIL checklist |
| `docs/TODO.md` | Agent task list (prioritised backlog) |

See [`AGENTS.md`](AGENTS.md), [`instructions.md`](instructions.md),
[`docs/security-flash-nvs.md`](docs/security-flash-nvs.md), and
[`docs/TODO.md`](docs/TODO.md) for coding rules, security, and backlog.

## Licence

Apache-2.0 — see [`LICENSE`](LICENSE).
