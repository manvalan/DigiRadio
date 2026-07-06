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

**Current firmware:** `0.8.0` — broadcast metadata (RDS PS/RT, DAB DLS),
preset reorder, CI gate (Doxygen + host tests + manual sync).

---

## Completed (fw 0.7.0–0.7.2)

- **Broadcast metadata (T4)** — RDS PS/RT and DAB DLS in
  `/api/tuner/status`, core parsers, Si4684 driver hook, UI lines.
- **BT1035 pairing** — `AT+PAIR`, `AT+A2DPSTAT`, `AT+A2DPDISC`,
  `BluetoothService`, REST + UI (not numbered below; landed with Slice 7).

---

## P0 — Fix the build gate (do this first)

### T1. Clear the 16 Doxygen warnings — **DONE (fw 0.7.1)**
Fixed invalid `\texttt`/`\r`/`\ref` in doc blocks; documented
`Station` accessors and `NetBootstrap`/`SetupWebServer` parameters.
`doxygen Doxyfile` exits 0 with an empty warnings log.

### T2. Add the CI workflow — **DONE (fw 0.7.1)**
`.github/workflows/ci.yml`: host `ctest`, Doxygen, manual sync on every
push/PR to `main`.

---

## P1 — Missing domain features

### T3. Station / preset list — polish — **DONE (fw 0.7.2)**
Reorder API (`POST /api/stations/reorder`), DAB playing ids in tuner
status and preset save, UI Up/Dn, host tests. **Remaining:** device HIL
(preset survives reboot) — manual only.

### T4. Broadcast metadata (RDS / DLS) — **DONE (fw 0.8.0)**
`BroadcastLabel`, RDS accumulator, DAB DLS accumulator, driver
`readDabServiceData`, status JSON fields, UI now-playing lines, host tests.

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
