# DigiRadio Firmware

Open-source Hi-Fi DAB+/FM receiver firmware for the ESP32-S3.

**Status:** Slice 3 — Si4684 + ADAU1701 boot at power-up (network from Slice 2).

## Quick start

Open this directory (`Software/`) as the Cursor project so `AGENTS.md` and
`.cursor/rules/` load automatically.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <port> flash monitor
```

Host unit tests (pure core, no hardware):

```bash
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Documentation (must exit 0):

```bash
doxygen Doxyfile
python3 tools/check-manual-sync.py
```

Manual PDF (design + HTTP API + class reference):

```bash
cd docs/manual && latexmk -lualatex manual.tex
```

## HTTP API (fw 0.3.0)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/health` | `{"status":"ok","fw":"0.3.0"}` |
| POST | `/api/wifi` | Provision STA credentials; reboot on success |

Full schemas, error tokens, and boot flow: [`docs/manual/ch-api.tex`](docs/manual/ch-api.tex).
C++ signatures: generate with `doxygen Doxyfile` → `docs/api/html/index.html`.

## Layout

| Path | Role |
|------|------|
| `Firmware/` | Si4684 `.bin` blobs (DAB+FM) + ADAU1701 SigmaStudio export |
| `components/drivers/si4684/` | AN649 SPI boot driver |
| `components/drivers/adau1701/` | I2C SigmaStudio RAM download |

See [`AGENTS.md`](AGENTS.md) §12 and [`instructions.md`](instructions.md) for
the authoritative repo layout, coding rules, and development roadmap.

## Licence

Apache-2.0 — see [`LICENSE`](LICENSE).
