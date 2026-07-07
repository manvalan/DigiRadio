# TODO — DigiRadio firmware

Agent task list and hardware-in-the-loop backlog. Working directory for all
commands is `Software/`.

**Current firmware:** `0.8.4` — dual OTA + DSP blob updates, EEPROM identity,
NVS + flash encryption (dev mode), tabbed Web UI with System uploads, CI gate
(4 jobs).

**Before writing code, read `AGENTS.md`, `.cursor/rules/`, and
`instructions.md`.** Definition of Done: Apache header, doc blocks,
`doxygen Doxyfile` exits 0, host tests pass, `check-manual-sync.py` and
`check_si4684_blobs.py` pass, no plaintext secrets.

---

## Completed agent tasks (T1–T12, fw 0.7.1–0.8.4)

| Task | Version | Summary |
|------|---------|---------|
| **T1** | 0.7.1 | Doxygen warnings cleared |
| **T2** | 0.7.1 | CI workflow (host tests, Doxygen, manual sync) |
| **T3** | 0.7.2 | Preset reorder API/UI, DAB playing ids in status |
| **T4** | 0.8.0 | RDS/DLS broadcast metadata |
| **T5** | 0.8.1 | `IntegrationService` — startup, preset recall, last-preset NVS |
| **T6** | 0.8.2 | Tabbed configuration Web UI (REST coverage) |
| **T7** | 0.8.2 | Si4684 blob policy — gitignore, docs, `check_si4684_blobs.py` |
| **T8** | 0.8.3 | NVS + flash encryption — `initEncryptedStorage`, security docs |
| **T9** | 0.8.4 | Dual-OTA partition table + `dsp` blob slot, rollback Kconfig |
| **T10** | 0.8.4 | EEPROM EUI-48 identity — SoftAP/BT/hostname/serial |
| **T11** | 0.8.4 | Updatable ADAU1701 program — `POST /api/dsp/program`, DRAD blob |
| **T12** | 0.8.4 | ESP32 OTA — `POST /api/system/ota`, rollback confirm on boot |

Also landed (not numbered): BT1035 pairing (`BluetoothService`), station presets
(fw 0.7.0), companion-chip boot (Slice 3), ADAU1701 runtime (Slice 5).

---

## P4 — Hardware-in-the-loop (when PCB arrives)

Manual validation only — does not block host CI.

### H1. Encrypted NVS boot path
Follow [`docs/security-flash-nvs.md`](security-flash-nvs.md): first flash with
`idf.py erase-flash flash`, verify boot logs, Wi-Fi provisioning survives
reboot, presets and `last_preset` survive power cycle.

### H2. End-to-end listening
Si4684 DAB/FM tune, ADAU1701 profile apply, BT1035 A2DP to headphones,
now-playing metadata in UI and `/api/tuner/status`.

### H3. OTA and DSP program update (on hardware)
Push a known-good `.bin` via `POST /api/system/ota`, confirm rollback after a
deliberately bad image. Upload a DRAD blob via `POST /api/dsp/program` and
verify ADAU replay after reboot.

### H4. Production flash encryption (optional)
After H1 passes, trial build with `sdkconfig.defaults.production` overlay on
a sacrificial unit; confirm RELEASE mode policy before shipping.

---

## Open firmware polish (non-blocking)

Done in fw 0.8.4 unless noted:

- FM seek down — `POST /api/tuner/seek` with `{"direction":"down"}`.
- BT1035 — query/set name, paired list (`AT+PLIST`), auto-reconnect
  (`AT+AUTOCONN`) per Feasycom BT1035 manual.
- Si4684 — `STOP_DIGITAL_SERVICE` (0x82) before FM band switch when DAB
  audio is active; ensemble metrics remain via `DAB_DIGRAD_STATUS` in status.

---

## Quality gates (run from `Software/` before merge)

```bash
cmake -S components/core/test -B build-host && cmake --build build-host
ctest --test-dir build-host --output-on-failure
doxygen Doxyfile
python3 tools/check-manual-sync.py
python3 tools/check_si4684_blobs.py
```

After editing the web UI: `tools/gzip-www.sh`.

---

## Notes for the agent

- Extend existing patterns (`AudioProfile` / `IAudioProfileStore` shape).
- Never invent Si4684 opcodes — cite AN649.
- Never invent BT1035 AT strings — cite Feasycom BT1035 programming guide.
- One logical change per commit; 50/72 messages.
- Update `ch-classes.tex` / `ch-api.tex` when public API or HTTP changes.
