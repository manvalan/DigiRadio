# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DigiRadio is an ESP32-S3 embedded Hi-Fi DAB+/FM radio firmware. Hardware companion chips:
- **Si4684** — DAB+/FM tuner over SPI (AN649 boot sequence)
- **ADAU1701** — SigmaDSP audio processor over I2C (safeload for click-free live updates)
- **FSC-BT1035** (QCC3056) — Bluetooth A2DP module over UART + AT commands, I2S slave from ADAU1701
- **24AA025E48** — EEPROM with factory EUI-48 for device identity

**Read AGENTS.md first** — it is the definitive coding rules and Definition of Done.

---

## Build & Development Commands

### Device Firmware (ESP-IDF v5.5.x)

```bash
idf.py set-target esp32s3          # first time only
idf.py build
idf.py erase-flash flash monitor   # first flash
idf.py flash monitor               # subsequent flashes
idf.py monitor                     # monitor only
```

Serial port: `/dev/cu.usbmodem1101`

### Host Unit Tests (no hardware required)

```bash
# Configure (once, or after CMakeLists changes)
cmake -S components/core/test -B build-host \
      -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"

# Build and run all tests
cmake --build build-host
ctest --test-dir build-host --output-on-failure

# Run a single test
ctest --test-dir build-host -R bt1035_at_test --output-on-failure
```

### Quality Gates (must all pass before commit)

```bash
ctest --test-dir build-host --output-on-failure
doxygen Doxyfile                        # Doxyfile has WARN_AS_ERROR = FAIL_ON_WARNINGS
python3 tools/check-manual-sync.py      # public classes must have manual sections
python3 tools/check_si4684_blobs.py     # blob checksums
```

### Other Tools

```bash
tools/gzip-www.sh                       # regenerate index.html.gz after editing the web UI
python3 tools/fetch_si4684_firmware.py  # download Si4684 firmware blobs (not in git)
```

---

## Architecture

### Layered Design

```
Imperative Shell   →  drivers/, services/, net/, main/
                              ↓ dependency injection
Application Services  →  TunerService, AudioService, BluetoothService, etc.
                              ↓ interface references
Pure Domain Core   →  components/core/   (zero ESP-IDF headers, host-testable)
```

The split is strict: **never add ESP-IDF includes to `components/core/`**. Domain logic lives there as pure C++23 with `std::expected<T,E>` for errors, no exceptions (`CONFIG_COMPILER_CXX_EXCEPTIONS=n`).

### Boot Flow

1. **`app_main()`** → `HardwareBootstrap::boot()`
   - Si4684: POWER_UP → stream ROM patch + image → BOOT
   - ADAU1701: RESET# → replay I2C cell writes from SigmaStudio export
   - BT1035: `AT+RESET` → `AT+AUXCFG=3` → `AT+I2SCFG=67` (I2S slave from ADAU) — **must appear in boot log**
   - 24AA025E48: read EUI-48 → derive serial number

2. **`secure_store::initEncryptedStorage()`** — encrypted NVS (dev mode: encryption off)

3. **`IntegrationService::bootLoadLastPreset()`** — restore saved frequency + audio profile + mixer

4. **`NetBootstrap::start()`** — STA (if creds saved) or SoftAP (`DigiRadio-<serial>`), HTTP server port 80

### GPIO Pinout (`main/board_pins.hpp` — single source of truth)

| Chip | Pins |
|------|------|
| Si4684 | SPI: SCLK=13 MOSI=12 MISO=9 CS=8, RSTB#=38, INTB#=39 |
| ADAU1701 | I2C SDA=4 SCL=5 RSTB#=47 addr=0x34 |
| BT1035 | UART TX=40 RX=41 RTS=14 CTS=21, RESET=17, SYS_CTL=15 |
| I2S | BCLK=6 (slave in), LRCLK=7 (slave in), DATA_OUT=16 |
| 24AA025E48 | I2C addr=0x52 (shared bus with ADAU1701) |

### Key Abstractions in `components/core/`

**Interfaces** (injected into services):
- `ITuner` — boot, tuneFm, tuneDab, seek, readRsq, readServices, playService
- `IDsp` — applyProfile, applyMixer, applyEq, setInputVolume, setMasterVolume (all safeload)
- `ISecureStore` — hasWifi, loadWifi, saveWifi, stations CRUD, audio profile
- `IFirmwareBlobReader` — stream blob chunks without heap allocation

**Value types**: `FrequencyKHz`, `GainDb`, `AudioProfile` (mixer + 6-band EQ + enhancements), `Station`, `StationList`, `DeviceIdentity`, `Secret` (no-log, zeroed on destruction)

**DSP math**: `BiquadDesign` converts Hz/Q/dB → ADAU1701 fixed-point coefficients (pure math, tested)

### Error Handling

All fallible operations return `std::expected<T, ErrorType>`. Core error types: `TunerError`, `DspError`, `Bt1035Error`, `StoreError`, `NetError`, `OtaError`. Never swallow errors.

### HTTP REST API

Served at port 80. Key routes:
- `GET /api/health` — chip status + serial number
- `POST /api/tuner/tune` — `{"band":"fm","frequency":100900}`
- `POST /api/tuner/seek` — `{"direction":"up"/"down"}`
- `GET|PUT /api/audio/profile` — full mixer + EQ JSON
- `POST /api/bluetooth/pair`, `GET /api/bluetooth/paired`, `POST /api/bluetooth/disconnect`
- `GET|POST /api/stations`, `POST /api/stations/tune`
- `POST /api/wifi` — STA provisioning (triggers reboot)
- `POST /api/system/ota` — firmware image upload (reboot)
- `POST /api/dsp/program` — ADAU1701 DRAD blob upload (reboot)

### Dual OTA & Partitions (`partitions.csv`)

Slots: `nvs`, `otadata`, `ota_0` (4 MB), `ota_1` (4 MB), `dsp` (40 KB blob), `nvs_keys`. `OtaService` streams to inactive slot, validates app descriptor, selects new boot slot.

---

## Important Files

| Path | Purpose |
|------|---------|
| `AGENTS.md` | Coding rules, DoD, subsystem specs — read first |
| `instructions.md` | Task roadmap |
| `main/board_pins.hpp` | GPIO pinout (single source of truth) |
| `main/hardware_bootstrap.cpp` | Driver boot orchestration |
| `sdkconfig.defaults` | C++23, exceptions off, dev encryption settings |
| `sdkconfig.defaults.production` | Flash encryption RELEASE mode |
| `partitions.csv` | Flash layout |
| `docs/security-flash-nvs.md` | NVS/flash encryption setup + HIL checklist |
| `.cursor/rules/` | Per-subsystem Cursor AI rules |
| `components/net/www/index.html` | Web UI source (gzip with `tools/gzip-www.sh` after edits) |

---

## Hard Rules

- **Never invent** register addresses, opcodes, or boot sequences. Cite the datasheet section (AN649 for Si4684; Feasycom guide §5.1 for BT1035).
- **No virtual calls** in IRAM ISRs (vtables in flash, inaccessible during cache disable).
- **No dynamic allocation** in audio hot paths.
- **No ESP-IDF includes** in `components/core/` — this breaks host tests.
- **Complexity ≤ 7** per method; method body fits an 80×24 terminal.
- Every public class needs a Doxygen block with `@dname`, `@param`, `@return`, `@pubstate`.
- Every file needs an Apache-2.0 license header.
