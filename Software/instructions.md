# instructions.md — DigiRadio firmware, agent kickoff

Read this together with `AGENTS.md` and everything under
`.cursor/rules/`. Those define *how* to write code; this file defines
*what we are building* and the current state on `main`.

**Firmware on `main`:** **0.9.0** — agent tasks T1–T12 complete; device HIL
now largely done on the first real PCB (see below), not pending anymore.

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

## Post-0.8.5 HIL work (first real PCB, not in the table above)

The board arrived and most of Slice 3–8's HIL assumptions turned out to be
wrong in ways that needed real fixes, not just testing. Summary (full
detail in `docs/si4684-rf-investigation-report.md`):

- **Si4684 total RF blackout, root-caused and fixed.** `writeCommand()`'s
  ARG1 byte was mis-offset across FM/DAB tune, seek, DAB service commands,
  and several ARG1-only status/ack commands — the chip answered every
  command correctly but never actually tuned. Real FM lock, real DAB
  ensemble lock (3+ ensembles), real audio confirmed live.
- **Si4684→ADAU1701 digital audio silence, fixed.** `PIN_CONFIG_ENABLE`
  had both I2SOUTEN and DACOUTEN set (chip falls back to unused analog
  out per AN649); `SerialInputRegister` IBP polarity was wrong (ADAU
  sampling on the wrong BCLK edge).
- **DAB service list, two rounds of offset bugs fixed.** Response-parsing
  offsets were wrong in a way that made `GET_DIGITAL_SERVICE_LIST` return
  empty/garbled; confirmed live with 22 real, correctly-decoded station
  labels. Also found `DAB_EVENT_INTERRUPT_SOURCE` (property 0xB300) was
  never configured, so the service-list-ready event could never fire.
- **BT1035 total boot silence, root cause found and fixed (2026-08-20).**
  Not a hardware fault: VBAT_IN/SYS_CTRL/VDD_IO/1.8V_OUT and TX/RX wiring
  were all independently confirmed correct with a multimeter (SYS_CTRL
  and 1.8V_OUT readings pinned down by probing the nearest decoupling cap
  instead of the tiny 0.5mm-pitch castellated pad directly, which had
  given a false "regulator dead" reading earlier). The actual bug: the
  module's spontaneous boot banner (`+VER=...`, `+DEVSTAT=1`) doesn't
  appear until ~18-24s after RESET# releases — full BT stack init, not
  just the internal regulator powering up. The old code only waited 3.5s
  before cutting power and restarting the whole sequence, so across every
  prior session the module never once got the chance to finish booting.
  Fixed by waiting up to 25s (`kBootBannerWaitMs`) for the banner before
  giving up; boot now succeeds on the first attempt, no retries needed.
- **FM front-end calibration.** The board's actual matching network
  differs from the AN851 reference the chip's auto-tune constants assume.
  Swept ANTCAP (AN851 Appendix A) and found a fixed override that beats
  auto-tune by 6–11 dB RSSI/SNR across the whole band; persisted to the
  24AA025E48 EEPROM (`POST /api/tuner/calibrate-antenna`) and applied by
  default to every FM tune.
- **New features, not in the original Slice plan:** full FM band scan
  (`POST /api/tuner/scan/full`), generic ADAU1701 parameter access
  (`GET`/`PUT /api/dsp/param`, an escape hatch onto any SigmaStudio cell
  beyond the curated mixer/EQ API), phone PCM streaming
  (`PUT /api/stream/phone`), BLE Wi-Fi provisioning
  (`net::ble_provisioning`, ESP-IDF's own `wifi_provisioning` over the
  ESP32-S3's onboard BLE, additive alongside the SoftAP), web radio
  streaming stutter fix (batched I2S writes).
- **Still open**: intermittent multi-second HTTP unresponsiveness under
  load (candidate cause: a blocking Si4684 SPI wait colliding with
  `max_open_sockets=3`); DAB signal quality still antenna-limited even
  after calibration; NVS partition (24 KB) may be undersized given the
  accumulated write traffic (`saveProfile()` `store_failed` seen
  intermittently, never root-caused).

Next work: keep chasing the open items above as the user prioritises them,
not new features unless requested.

- **Blockers first** — state risks before solutions.
- **One vertical slice at a time** — `main` always builds; host tests green.
- Apache header + Doxygen doc blocks on every file/class/method.
- Never invent register/opcode/boot steps — cite the datasheet or stop.
- After changes: `ctest`, `doxygen`, `check-manual-sync.py`, `check_si4684_blobs.py`.

## Slice 1 — Walking skeleton (complete)

- ESP-IDF `esp32s3`, C++23, `components/core` host-testable.
- SoftAP `DigiRadio-<suffix>` (or setup fallback), gzipped page, `GET /api/health`.
- Health JSON includes `fw` (today **0.8.5**), `serialNumber`, companion-chip flags.
- BT1035 boot must send `AT+AUXCFG=3` and `AT+I2SCFG=67` (I2S from ADAU1701).

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
