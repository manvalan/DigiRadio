# TODO — DigiRadio firmware

Task list for the coding agent. Work top to bottom; each task is a
vertical slice that keeps `main` building and the host tests green.

**Before writing code, read `AGENTS.md`, `.cursor/rules/`, and
`instructions.md`.** Every task below must satisfy the Definition of Done
in `AGENTS.md §10`: Apache header on new files, doc block on every class
and method, `doxygen Doxyfile` exits 0, host tests pass,
`tools/check-manual-sync.py` passes, no primitive obsession, typed
errors, no plaintext secrets.

Working directory for all commands is `Software/`.

**Current firmware:** `0.7.0` — BT1035 pairing (`/api/bluetooth/*`),
station presets (`/api/stations/*`), host tests green (10/10), manual
sync green (38 classes).

---

## Completed (fw 0.7.0)

- **Station / preset list (T3 core)** — `Station`, `StationList`, NVS key
  `station_list`, `StationService`, REST + Presets UI. Remaining polish:
  reorder API, save DAB `service_id`/`component_id` from UI, HIL on device.
- **BT1035 pairing** — `AT+PAIR`, `AT+A2DPSTAT`, `AT+A2DPDISC`,
  `BluetoothService`, REST + UI (not numbered below; landed with Slice 7).

---

## P0 — Fix the build gate (do this first)

### T1. Clear the 16 Doxygen warnings
**Why:** `doxygen Doxyfile` currently exits non-zero, so the docs gate is
red and CI (once added) will fail.
**What:**
- Replace the invalid `\texttt{...}` with `` `...` `` (backticks) or
  `\c word` in: `core/AudioProfileJson.hpp` (l.58, l.80),
  `core/AudioEnhancements.hpp` (l.25).
- Fix `\r` interpreted as a command in `core/Bt1035At.hpp` (l.64) —
  wrap the AT string in `@code ... @endcode` or escape as `\\r`.
- Add a space after `\ref` in `bt1035/Bt1035Driver.hpp` (l.89) and
  `core/AudioEnhancements.hpp` (l.24).
- Document the missing `@param companionChips` in
  `net/NetBootstrap.hpp` (`start`) and `net/SetupWebServer.hpp` (`start`).
**Done when:** `doxygen Doxyfile` exits 0 and `docs/api/doxygen-warnings.log`
is empty.

### T2. Add the CI workflow
**Why:** validate every push automatically.
**What:** add `.github/workflows/ci.yml` with three jobs — host build +
`ctest`, `doxygen` (fail on warnings), and `check-manual-sync.py`. A ready
draft was prepared; place it and confirm all three jobs pass.
**Done when:** the workflow is green on `main` after T1.

---

## P1 — Missing domain features

### T3. Station / preset list — polish  *(core done in 0.7.0)*
**Status:** CRUD, NVS persistence, tune recall, and basic UI are shipped.
**Remaining:**
- `POST /api/stations/reorder` (core has `StationList::move()`).
- Save DAB presets with `service_id` / `component_id` from last played service.
- Device HIL: preset survives reboot, tune recall on hardware.
**Done when:** the gaps above are closed and covered by tests.

### T4. Broadcast metadata (RDS / DLS)
**Why:** `TunerStatus` currently carries only a 17-char `label`. Real
radio UX needs the FM RDS station name/radiotext and DAB DLS dynamic
label so the UI can show "what's playing".
**What:**
- Extend the tuner data model with structured metadata (station name,
  radiotext/dynamic label), read from the Si4684 in the driver.
- Surface it through `TunerService::refreshStatus` and the
  `/api/tuner/status` JSON.
- Keep the Si4684 register/command details in the driver; the core model
  stays hardware-free.
**Done when:** the status JSON exposes the metadata and host tests cover
the parsing of raw label bytes into the model.

### T5. Remove the services stub — integration service
**Why:** `components/services/src/component_stub.cpp` is a Slice-7
placeholder. Tuner and Audio services exist separately but nothing
orchestrates them together.
**What:** implement the integration layer that binds `TunerService`,
`AudioService`, the station list, and the network layer into the
application flow (e.g. "select a preset → tune → apply the stored audio
profile"). Replace the stub file.
**Done when:** `app_main` drives a real end-to-end flow through this
service; the stub is gone; a manual section documents the new class.

---

## P2 — User interface

### T6. Complete the configuration Web UI
**Why:** `components/net/www/index.html` is functional but the
requirement is an *elegant, essential* SPA. It should cover:
Wi-Fi provisioning, station list management, tuner control (FM/DAB,
seek, play), live EQ/volume/enhancement control, Bluetooth pairing, and
now-playing metadata.
**What:** a minimal single-page app served gzipped from flash, design
tokens defined once (spacing, type scale, one accent), thin client over
the existing typed JSON API. No heavy frameworks. No business logic in
the UI.
**Done when:** every API capability has a UI control; the page is served
gzipped; no debug endpoint is exposed in a shipping build.

---

## P3 — Procurement & hardening

### T7. Si4684 firmware blob strategy (legal)
**Why:** the Si4684 images are proprietary (Skyworks). Tools to fetch/
extract exist under `tools/`, but the `.bin` images must **not** be
committed to the public repo.
**What:**
- Confirm `*.bin` (patch, FM, DAB images) are in `.gitignore` and absent
  from git history.
- Document in `Software/Firmware/Si4684-Firmware/README.md` how a builder
  obtains the images locally (tools + AN649 reference), without
  redistributing proprietary binaries.
**Done when:** no proprietary blob is tracked; the procurement path is
documented and reproducible.

### T8. Flash/NVS encryption enablement
**Why:** secure storage holds Wi-Fi and user credentials; encryption at
rest was deferred.
**What:** enable NVS encryption on an encrypted partition (with flash
encryption), per current ESP-IDF security docs. Verify the mechanism
before enabling; keep keys out of the repo.
**Done when:** credentials are encrypted at rest and the boot path still
loads them.

---

## Notes for the agent
- Prefer extending existing patterns over inventing new ones: copy the
  shape of `AudioProfile` / `AudioProfileJson` / `IAudioProfileStore` for
  new persisted models.
- Never invent Si4684 register/command details — cite AN649.
- One logical change per commit; 50/72 commit messages.
- After each task, run: host `ctest`, `doxygen Doxyfile`,
  `tools/check-manual-sync.py` — all must pass before moving on.
