# TODO — DigiRadio firmware

Agent task list and hardware-in-the-loop backlog. Working directory for all
commands is `Software/`.

**Current firmware:** `0.9.0` — everything in 0.8.5, plus: Si4684 RF
blackout root-caused and fixed (real FM/DAB lock and audio on real
hardware), DAB service list fixed (two rounds), FM ANTCAP antenna
calibration persisted to EEPROM, generic ADAU1701 parameter API, phone PCM
streaming, BLE Wi-Fi provisioning, full FM band scan, BT1035 boot retry.
See "Post-0.8.5 hardware-in-the-loop findings" below and
`docs/si4684-rf-investigation-report.md` for the full story.

**Before writing code, read `AGENTS.md`, `.cursor/rules/`, and
`instructions.md`.** Definition of Done: Apache header, doc blocks,
`doxygen Doxyfile` exits 0, host tests pass, `check-manual-sync.py` and
`check_si4684_blobs.py` pass, no plaintext secrets.

---

## Completed agent tasks (T1–T12, fw 0.7.1–0.8.5)

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

**0.8.5** (hardware/doc alignment): BT1035 boot uses `AT+AUXCFG=3` +
`AT+I2SCFG=67` (I2S from ADAU1701, not Line-In); I2C pull-ups R1/R16
confirmed 2\,kΩ; `Hardware/DATASHEET/` bundle + manual cross-refs.

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

### H5. Si4684 FM/DAB no-lock — RESOLVED, was firmware after all
**Superseded verdict (2026-08-13): blob OK → suspected U6 RF ground, PCBWay
dispute opened.** That verdict was wrong. The actual cause was
`writeCommand()`'s ARG1 byte being mis-offset across FM/DAB tune, seek, and
several status/ack commands — the chip always answered correctly, so every
signal pointed at hardware, but it never actually tuned. Fixed; real FM
lock, real DAB ensemble lock, real audio confirmed live on the same board.
No PCB rework was needed. Full investigation, the wrong initial verdict,
and the eventual root cause: [`docs/si4684-rf-investigation-report.md`](si4684-rf-investigation-report.md).

---

## Post-0.8.5 hardware-in-the-loop findings

The board arrived and testing against it (not just host tests) found real
bugs the host-testable core couldn't catch, since they live in
ESP-IDF-only drivers. Full detail and evidence in
[`docs/si4684-rf-investigation-report.md`](si4684-rf-investigation-report.md).
Short version:

- Si4684 total RF blackout (H5 above) — firmware bug, fixed.
- Si4684→ADAU1701 digital audio silence — `PIN_CONFIG_ENABLE` mutual
  exclusion + `SerialInputRegister` polarity, fixed.
- DAB service list empty/garbled — response-parsing offset bugs (two
  rounds) plus `DAB_EVENT_INTERRUPT_SOURCE` (0xB300) never configured,
  fixed.
- FM front-end auto-tune measurably suboptimal on this board's actual
  matching network — ANTCAP calibration swept and persisted to EEPROM,
  `POST /api/tuner/calibrate-antenna`.
- BT1035 total boot silence — root cause found and fixed (2026-08-20):
  the module's spontaneous boot banner (`+VER=...`, `+DEVSTAT=1`) doesn't
  appear until ~18-24s after RESET# releases (full BT stack init, not
  just the internal regulator), but the boot code only waited 3.5s before
  cutting power and restarting — so every attempt, in every prior session,
  cut power before the module could ever finish booting even once. Power
  rails (VBAT_IN/SYS_CTRL/VDD_IO/1.8V_OUT) and TX/RX wiring were all
  independently verified correct with a multimeter first — the module and
  PCB were never at fault. Fixed by waiting up to 25s for the banner
  (`kBootBannerWaitMs`); boot now succeeds on the first attempt.
- **Still open**: intermittent multi-second HTTP unresponsiveness under
  load; DAB signal quality still antenna-limited; 24 KB `nvs` partition
  may be undersized (`saveProfile()` `store_failed` seen intermittently,
  error code never captured).

---

## Open firmware polish (non-blocking)

Done in fw 0.8.5 unless noted:

- BT1035 boot — I2S slave init (`AT+AUXCFG=3`, `AT+I2SCFG=67`) per PCB routing (0.8.5).
- FM seek down — `POST /api/tuner/seek` with `{"direction":"down"}` (0.8.4).
- BT1035 — query/set name, paired list (`AT+PLIST`), auto-reconnect
  (`AT+AUTOCONN`) per Feasycom BT1035 manual (0.8.4).
- Si4684 — `STOP_DIGITAL_SERVICE` (0x82) before FM band switch when DAB
  audio is active; ensemble metrics remain via `DAB_DIGRAD_STATUS` in status (0.8.4).

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
