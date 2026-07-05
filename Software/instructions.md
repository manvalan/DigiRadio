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

1. **Walking skeleton** — boot, a task, SoftAP, web server, one JSON
   endpoint, one host test, docs green. No chip drivers yet. (Spec below.)
2. Secure store (`ISecureStore`) + Wi-Fi provisioning UI (STA join).
3. Station/frequency list model + persistence + UI.
4. Si4684 driver: power-up, load image, tune, read RSQ.
5. ADAU1701 driver: RAM boot, then safeload EQ + input mixer.
6. FSC-BT1035 driver: AT init (incl. `AT+AUXCFG=1`), audio out.
7. Integration: TunerService + AudioService end to end.

## Slice 1 — Walking skeleton (the first task)

Goal: exercise the whole toolchain end to end with zero chip hardware,
so every later slice drops into a working frame.

Build:
- Top-level ESP-IDF project targeting `esp32s3`.
- `sdkconfig.defaults` sets C++23, exceptions off, and the flash/NVS
  encryption options (leave encryption keys/enablement documented, not
  hard-enabled, until we decide on the secure-store slice).
- The `components/core` component compiles both under ESP-IDF and
  standalone on the host.

Behaviour:
- `app_main` starts a FreeRTOS task that logs a heartbeat on a timer.
- Bring up SoftAP with a known SSID (e.g. `DigiRadio-setup`).
- Start an HTTP server serving one minimal gzipped page from flash.
- Expose `GET /api/health` returning a typed DTO serialised by the pure
  core, e.g. `{"status":"ok","fw":"0.1.0"}`.

Acceptance criteria:
- [ ] `idf.py build` succeeds; `flash monitor` shows the heartbeat.
- [ ] A phone/laptop can join the SoftAP, load the page, and get a valid
      JSON response from `/api/health`.
- [ ] The health DTO is defined and serialised in `components/core`,
      with a host unit test that passes under `ctest`.
- [ ] `doxygen Doxyfile` exits 0 with an empty warnings log.
- [ ] Every file has the Apache header; every class/method its doc block.
- [ ] No ESP-IDF headers included from `components/core`.

Out of scope for Slice 1: any Si4684 / ADAU1701 / BT1035 code, real
credentials, encryption enablement. Those come in later slices.

## First message to the agent

Ask it to read `AGENTS.md`, `.cursor/rules/`, and this file, then
respond with: (1) the confirmed stack, (2) the exact repo layout it will
create, (3) how it will satisfy each Slice 1 acceptance criterion, and
(4) any blockers or questions — **before** writing code.
