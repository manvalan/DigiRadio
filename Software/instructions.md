# instructions.md — DigiRadio firmware, agent kickoff

Read this together with `AGENTS.md` and everything under
`.cursor/rules/`. Those define *how* to write code; this file defines
*what we are building* and the current state on `main`.

**Firmware on `main`:** **0.8.4** — agent tasks T1–T12 complete; device HIL
pending PCB arrival.

## What DigiRadio is

An open-source Hi-Fi DAB+/FM digital radio board. Firmware runs on an
ESP32-S3 and coordinates three companion chips:
- **Si4684** — DAB+/FM tuner (delivers the audio stream).
- **ADAU1701** — SigmaDSP: equaliser + input mixer between the Si4684
  and the ESP32 audio path. Program is written to DSP RAM at every boot
  (no self-boot EEPROM).
- **FSC-BT1035 (QCC3056)** — Bluetooth 5.2 out with aptX Adaptive,
  controlled by AT commands over UART.

Plus: tabbed web UI for provisioning and control; **encrypted NVS** for
Wi-Fi credentials, presets, audio profiles, and last-preset index.

Repository: https://github.com/manvalan/DigiRadio

## Confirmed technical decisions (do not re-litigate)

| Area        | Decision                                              |
|-------------|-------------------------------------------------------|
| Framework   | ESP-IDF v5.5.x (native, not Arduino)                  |
| Language    | C++23, pinned `-std=gnu++23`                          |
| Errors      | `std::expected<T, Error>` (native); exceptions OFF    |
| DSP boot    | ESP32 writes ADAU1701 RAM at every boot (no EEPROM)   |
| Architecture| Functional core (pure, host-tested) + imperative shell|
| Security    | NVS + flash encryption (dev mode); see `docs/security-flash-nvs.md` |
| Docs        | Doxygen + LaTeX manual sync (CI enforced)             |
| HW licence  | CERN-OHL-S v2 · FW licence: Apache-2.0                |

## Roadmap status

| Slice / task | Status | Notes |
|--------------|--------|-------|
| 1 Walking skeleton | Done | SoftAP, gzipped UI, `/api/health` |
| 2 Secure store + Wi-Fi | Done | `ISecureStore`, STA provisioning |
| 3 Companion-chip boot | Done | Si4684 + ADAU1701 from `Firmware/` |
| 4 Station presets | Done (0.7.0) | NVS `station_list`, full `/api/stations/*` |
| 5 ADAU1701 runtime | Done | EQ, mixer, enhancements, audio API |
| 6 Si4684 tuning | Done | FM/DAB tune, seek, RSQ, RDS, DAB services/DLS |
| 7 BT1035 | Done (0.8.4) | Pairing, A2DP, name/plist/auto-reconnect AT |
| 8 Integration | Done (0.8.1) | `IntegrationService`, last-preset NVS |
| T6 Web UI | Done (0.8.4) | Tabbed SPA + System tab (OTA/DSP upload) |
| T7 Si4684 blobs | Done (0.8.2) | Local-only `.bin`, CI policy check |
| T8 NVS encryption | Done (0.8.3) | `initEncryptedStorage`; HIL when PCB ready |
| T9–T12 Platform | Done (0.8.4) | Dual OTA, EEPROM identity, DSP + firmware OTA |

Next work: **hardware-in-the-loop** (`docs/TODO.md` § P4), not new features
unless the user requests them.

- **Blockers first** — state risks before solutions.
- **One vertical slice at a time** — `main` always builds; host tests green.
- Apache header + Doxygen doc blocks on every file/class/method.
- Never invent register/opcode/boot steps — cite the datasheet or stop.
- After changes: `ctest`, `doxygen`, `check-manual-sync.py`, `check_si4684_blobs.py`.

## Slice 1 — Walking skeleton (complete)

- ESP-IDF `esp32s3`, C++23, `components/core` host-testable.
- SoftAP `DigiRadio-<suffix>` (or setup fallback), gzipped page, `GET /api/health`.
- Health JSON includes `fw` (today **0.8.4**), `serialNumber`, companion-chip flags.

## Slice 2 — Secure store + Wi-Fi STA (complete)

- `ISecureStore`, `NvsSecureStore`, `StaClient`, `NetBootstrap`.
- `POST /api/wifi` + Wi-Fi tab in web UI.
- NVS encryption enabled in fw 0.8.3 via `initEncryptedStorage()`.

## Slice 3 — Companion-chip boot (complete)

- Si4684 blobs local-only (`tools/fetch_si4684_firmware.py`).
- `Si4684Driver`, `Adau1701Driver`, `HardwareBootstrap` before network.
- Device flash: pending HIL on first PCB.

## Slices 4–8 — Presets, audio, tuner, BT, integration (complete)

- Presets: `StationService`, reorder, integration recall with audio profile.
- Audio: six-band EQ, enhancements, `NvsAudioProfileStore`.
- Tuner: RDS/DLS metadata in status JSON and Now Playing UI.
- Bluetooth: `BluetoothService`, pairing REST + UI.
- Integration: boot loads last preset; `POST /api/stations/tune` orchestrates tune + audio + NVS.

## Quality gates (from `Software/`)

```bash
cmake -S components/core/test -B build-host && cmake --build build-host
ctest --test-dir build-host --output-on-failure
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
```

First device flash with encryption: `idf.py erase-flash flash` — see
`docs/security-flash-nvs.md`.
