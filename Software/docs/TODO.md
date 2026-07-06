# DigiRadio — consolidated TODO (audit 2026-07-06)

Firmware **0.7.0** after BT1035 pairing + Slice 4 presets. This list
cross-checks code, manual, API, UI, and `instructions.md`.

## Done in this slice

| Area | Status |
|------|--------|
| BT1035 pairing AT (`AT+PAIR`, `AT+A2DPSTAT`, `AT+A2DPDISC`) | Done |
| `BluetoothService` + `/api/bluetooth/*` + UI | Done |
| `Station`, `StationList`, NVS persistence | Done |
| `StationService` + `/api/stations/*` + UI | Done |
| Host tests (10/10 green) | Done |
| Manual: `ch-api`, `ch-bt1035`, `ch-classes` | Done |

## High priority (next)

1. **Device flash / HIL** — verify Si4684, ADAU1701, BT1035 on hardware; pairing with real headphones; preset recall across reboot.
2. **DAB preset save from UI** — “Save current tune target” stores ensemble index only; capture `service_id` / `component_id` from the last played DAB service (needs UI state or tuner cache).
3. **FM band switch UX** — Si4684 boots DAB; FM tune may reload FM image; document/limit band changes in UI (auto-reload or explicit band selector).
4. **NVS encryption** — enable `nvs_keys` partition for production (Wi-Fi, presets, future user creds).
5. **Slice 8 integration** — unify tuner + audio + presets in a single “now playing” model; source selection (DAB / FM / BT Line-In).

## Medium priority

6. **IBtModule interface** — AGENTS mentions it; driver is used directly today. Add when a second BT module or host fake is needed.
7. **BT1035 extended AT** — `AT+NAME`, `AT+PLIST`, `AT+A2DPCONN`, event-driven `+PAIRED` / `+A2DPDEV` (UART listener task).
8. **Station list reorder API** — `StationList::move()` exists in core; no HTTP route yet.
9. **EQ UI completeness** — web UI exposes master/mixer/enhance; per-band EQ editing not in UI (API supports full profile PUT).
10. **Si4684 RSQ / scan UX** — driver + HTTP largely done; polish seek, service list refresh, signal display on UI.
11. **User credentials** — `ISecureStore` extension for login (out of scope until product needs it).

## Documentation / tooling

12. **Overleaf sync** — GitHub is source of truth; root `docs/` symlink can break Overleaf push; compile from `Software/docs/manual/manual.tex`.
13. **Firmware version single source** — `0.7.0` in `SetupWebServer.cpp`; align `FirmwareVersion` / health test constants if desired.
14. **Doxygen pass** — run `doxygen Doxyfile` on CI host with ESP-IDF toolchain.

## Low priority / ideas

15. Physical preset buttons → map GPIO to `StationService::tuneToIndex` by `PresetSlot`.
16. OTA updates, mDNS hostname (`digiradio.local`), HTTPS on LAN.
17. aptX license note (Feasycom) — commercial firmware variant if needed.

## Test gaps

| Missing test | Layer |
|--------------|-------|
| `BluetoothService` with fake driver | Host (needs `IBtModule` or inject interface) |
| `StationService` + fake `ISecureStore` | Host |
| `NvsSecureStore` station round-trip | Target / integration |
| BT1035 UART HIL | Hardware-only, marked separate |

## Roadmap alignment (`instructions.md`)

| Slice | Item | State |
|-------|------|-------|
| 4 | Station list + persistence + UI | **Done** (basic CRUD + tune) |
| 5 | Si4684 tuning / RSQ / DAB properties | Mostly done; UI polish open |
| 7 | BT1035 pairing | **Done** (discover + A2DP stat/disconnect) |
| 8 | TunerService + AudioService E2E | Partial — health chips OK; unified UX open |
