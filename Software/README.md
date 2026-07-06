# DigiRadio Firmware

Open-source Hi-Fi DAB+/FM receiver firmware for the ESP32-S3.

**Status:** Slice 2 — secure store and Wi-Fi STA provisioning.

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

## HTTP API (fw 0.2.0)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/health` | `{"status":"ok","fw":"0.2.0"}` |
| POST | `/api/wifi` | Provision STA credentials; reboot on success |

Full schemas, error tokens, and boot flow: [`docs/manual/ch-api.tex`](docs/manual/ch-api.tex).
C++ signatures: generate with `doxygen Doxyfile` → `docs/api/html/index.html`.

## Layout

See [`AGENTS.md`](AGENTS.md) §12 and [`instructions.md`](instructions.md) for
the authoritative repo layout, coding rules, and development roadmap.

## Licence

Apache-2.0 — see [`LICENSE`](LICENSE).
