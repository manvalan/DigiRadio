# instructions.md — DigiRadio firmware, agent kickoff

Read this together with `AGENTS.md` and everything under
`.cursor/rules/`. Those define *how* to write code; this file defines
*what we are building* and *what to do first*.

## What DigiRadio is

An open-source Hi-Fi DAB+/FM digital radio board. Firmware runs on an
ESP32-S3 and coordinates three companion chips:
- **Si4684** — DAB+/FM tuner (delivers the audio stream).
- **ADAU1701** — SigmaDSP: equaliser + input mixer between the Si4684
  and the ESP32 audio path. Program is written to DSP RAM at every boot
  (no self-boot EEPROM).
- **FSC-BT1035 (QCC3056)** — Bluetooth 5.2 out with aptX Adaptive,
  controlled by AT commands over UART.

Plus: an elegant, essential web UI for network configuration; encrypted
storage for Wi-Fi and user credentials and the station list.

Repository: https://github.com/manvalan/DigiRadio

## Confirmed technical decisions (do not re-litigate)

| Area        | Decision                                              |
|-------------|-------------------------------------------------------|
| Framework   | ESP-IDF v5.5.x (native, not Arduino)                  |
| Language    | C++23, pinned `-std=gnu++23`                          |
| Errors      | `std::expected<T, Error>` (native); exceptions OFF    |
| DSP boot    | ESP32 writes ADAU1701 RAM at every boot (no EEPROM)   |
| Architecture| Functional core (pure, host-tested) + imperative shell|
| Docs        | Doxygen, build must exit 0 (enforced)                 |
| HW licence  | CERN-OHL-S v2 · FW licence: Apache-2.0                |

## Working agreement

- **Confirm understanding before writing code.** On kickoff, summarise
  the plan and list any blockers or unclear hardware invariants first.
- **Blockers first**, always. State risks before solutions.
- **One vertical slice at a time.** `main` always builds and runs.
- Every file gets the Apache header; every class/method its doc block;
  `doxygen Doxyfile` stays green. Small commits, 50/72 messages.
- Never invent a register/opcode/boot step — cite the datasheet or stop.

## Roadmap (slices, in order)

1. **Walking skeleton** — done (Slice 1).
2. **Secure store + Wi-Fi provisioning** — done (Slice 2).
3. **Companion-chip boot** — done (Slice 3): Si4684 DAB + ADAU1701 RAM load.
4. Station/frequency list model + persistence + UI.
5. Si4684 tuning: RSQ, station list, DAB properties.
6. **ADAU1701 runtime** — done (Slice 5): safeload EQ + input mixer + HTTP.
7. FSC-BT1035 driver: AT init (incl. `AT+AUXCFG=1`), audio out.
8. Integration: TunerService + AudioService end to end.

## Slice 1 — Walking skeleton (complete)

Goal: exercise the whole toolchain end to end with zero chip hardware,
so every later slice drops into a working frame.

Build:
- Top-level ESP-IDF project targeting `esp32s3`.
- `sdkconfig.defaults` sets C++23, exceptions off, and documents flash/NVS
  encryption options (not hard-enabled until production).
- The `components/core` component compiles both under ESP-IDF and
  standalone on the host.

Behaviour:
- `app_main` starts a FreeRTOS task that logs a heartbeat on a timer.
- Bring up SoftAP with a known SSID (e.g. `DigiRadio-setup`).
- Start an HTTP server serving one minimal gzipped page from flash.
- Expose `GET /api/health` returning a typed DTO serialised by the pure
  core, e.g. `{"status":"ok","fw":"0.3.0"}`.

Documentation (required):
- Doxygen doc blocks on every class/method; `doxygen Doxyfile` green.
- Manual: class sections in `docs/manual/ch-classes.tex`;
  HTTP API in `docs/manual/ch-api.tex`.
- `python3 tools/check-manual-sync.py` green.

## Slice 2 — Secure store + Wi-Fi STA (complete)

Goal: persist Wi-Fi credentials and join the configured network after
provisioning; fall back to SoftAP when no credentials or join fails.

Build on Slice 1:
- `core::ISecureStore` interface + `secure_store::NvsSecureStore` (NVS).
- `core::Secret`, `WifiSsid`, `WifiCredentials`, `parseWifiProvisionJson`.
- `net::StaClient`, `NetBootstrap::start(store)` state machine.
- `POST /api/wifi` + provisioning form in the web UI; reboot after save.

Acceptance criteria:
- [x] Provisioning via SoftAP saves credentials and reboots; next boot joins STA.
- [x] Host tests for health JSON and Wi-Fi provision parse/serialise.
- [x] Doxygen green; manual sync green; `ch-api.tex` documents endpoints.
- [x] No ESP-IDF headers in `components/core`.

Out of scope: station list, user credentials, NVS encryption enablement
(production), chip drivers.

## Slice 3 — Companion-chip boot (complete)

Goal: load Si4684 DAB firmware and ADAU1701 SigmaStudio program from
`Firmware/` on every boot, before network bring-up.

Build:
- `Firmware/Si4684-Firmware/` — `rom_patch_016.bin`, `dab_firmware.bin`,
  `fm_firmware.bin` (FM via `tools/fetch_si4684_firmware.py --si46xx-dir`).
- `Firmware/ADAU1701-Firmware/` — SigmaStudio export (`DigiRadio_IC_1.h`, …).
- `core::IFirmwareBlobReader` + `EmbeddedBlobReader` for chunked HOST_LOAD.
- `si4684::Si4684Driver`, `adau1701::Adau1701Driver`, `HardwareBootstrap`.

Behaviour:
- `app_main` calls `HardwareBootstrap::boot()` first (Si4684, then ADAU1701).
- On failure, firmware logs and halts before Wi-Fi.

Acceptance criteria:
- [x] AN649 boot sequence with streaming blobs (no full image on heap).
- [x] ADAU1701 reset + I2C + `default_download_IC_1()` replay.
- [x] Host test for `EmbeddedBlobReader`; manual sync green.
- [ ] Device flash verified (requires ESP-IDF toolchain on build host).

## Slice 5 — ADAU1701 runtime + audio API (complete)

Goal: safeload mixer/EQ/master on the ADAU1701 at runtime; persist user
profiles in NVS; expose REST and web UI controls.

Build on Slice 3–4:
- Pure core: `GainDb`, `EqProfile`, `AudioProfile`, `IDsp`, biquad design,
  `parseAudioProfileJson` / `serializeAudioProfileJson`.
- Driver: `sigma_safeload_*`, extended `Adau1701Driver`, `Adau1701Dsp`.
- Service: `audio::AudioService`, `secure_store::NvsAudioProfileStore`.
- HTTP: `GET/PUT /api/audio/profile`, `POST /api/audio/reset`; Audio
  section in the web UI. Firmware **0.5.0**.

Acceptance criteria:
- [x] Safeload volume/mixer/EQ without direct param RAM writes during audio.
- [x] Profile load/apply after ADAU boot; NVS round-trip via JSON.
- [x] Host tests for biquad fixpoint and audio profile JSON.
- [x] Doxygen green; manual sync green; `ch-api.tex` documents audio routes.
- [ ] Device flash verified on hardware.
