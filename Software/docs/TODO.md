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

**Current firmware:** `0.8.3` — NVS + flash encryption (dev mode), tabbed Web
UI, integration service, CI gate.

---

## Completed (fw 0.7.0–0.8.3)

- **Integration service (T5)** — preset recall with audio profile re-apply,
  last-preset NVS, \texttt{app\_main} orchestration.
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

### T5. Remove the services stub — integration service — **DONE (fw 0.8.1)**
`integration::IntegrationService` orchestrates startup, preset recall,
audio profile re-apply, and last-preset NVS. Stub removed; `app_main` and
`POST /api/stations/tune` delegate here.

---

## P2 — User interface

### T6. Complete the configuration Web UI — **DONE (fw 0.8.2)**
Tabbed SPA (`Now` / `Radio` / `Presets` / `Audio` / `BT` / `Wi‑Fi`):
now-playing hero with 5 s metadata poll, six-band EQ sliders, all REST
endpoints wired, companion-chip badges, `tools/gzip-www.sh` for the
embedded gzip blob. No debug routes in `SetupWebServer`.

---

## P3 — Procurement & hardening

### T7. Si4684 firmware blob strategy (legal) — **DONE (fw 0.8.2)**
`Firmware/Si4684-Firmware/*.bin` gitignored; no blobs in git history.
Procurement documented in `Si4684-Firmware/README.md`; CI job
`si4684-blobs` runs `tools/check_si4684_blobs.py`.

### T8. Flash/NVS encryption enablement — **DONE (fw 0.8.3)**
`CONFIG_NVS_ENCRYPTION` + flash encryption (development mode) in
`sdkconfig.defaults`; `secure_store::initEncryptedStorage()`; production
overlay `sdkconfig.defaults.production`; HIL checklist in
`docs/security-flash-nvs.md`. **Pending:** device verification when PCB
arrives.

---

## Notes for the agent
- Prefer extending existing patterns over inventing new ones: copy the
  shape of `AudioProfile` / `AudioProfileJson` / `IAudioProfileStore` for
  new persisted models.
- Never invent Si4684 register/command details — cite AN649.
- One logical change per commit; 50/72 commit messages.
- After each task, run: host `ctest`, `doxygen Doxyfile`,
  `tools/check-manual-sync.py` — all must pass before moving on.
