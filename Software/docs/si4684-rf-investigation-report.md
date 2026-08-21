# Si4684 RF no-lock investigation — status report

**Date**: 2026-08-13
**Board**: PCBWay order W96157ASH49, U6 = Si4684-A10 (confirmed genuine, top marking `4684A10-2112AD254YZ-2112AD-E3`)

## Symptom

Si4684 (U6) boots, loads the ROM patch and FM/DAB application images, and answers
every SPI command correctly (CTS, boot sequence, property reads/writes all succeed).
FM and DAB tuning never completes: the STCINT status bit never sets, RSQ/DIGRAD
metrics stay at zero (RSSI=0, SNR=0, VALID=0, FIC quality=0, empty service list),
on both bands, at every frequency tried.

## What has been verified correct (do not re-litigate)

All cross-checked byte-by-byte against the official Skyworks documents
(`Hardware/DATASHEET/AN649.pdf`, `AN851_Schematics_Layout.pdf`):

- **Boot sequence**: RSTB# pulse, ROM patch stream, image stream, `BOOT` — matches
  the AN649 flowchart exactly, on every boot, both bands.
- **Crystal / POWER_UP**: `XTAL_FREQ` = 19,200,000 exact (bytes `00 F8 24 01`,
  little-endian `0x0124F800`), `CLK_MODE` = crystal mode (`0x17` → bits 5:4 = `01`).
  Matches the physical ABM8-19.200MHZ-10-1-U-T crystal (U7) and BOM/schematic.
- **I2S**: Si4684 configured as I2S slave (`DIGITAL_IO_OUTPUT_SELECT = 0x0000`),
  48 kHz; ADAU1701 is bus master. Confirmed via GPIO clock probe (LRCLK ≈ 48000 Hz,
  BCLK ≈ 3.07 MHz) at every boot.
- **Front-end matching properties**: `FM/DAB_TUNE_FE_VARM` (0x1710),
  `FM/DAB_TUNE_FE_VARB` (0x1711), and `FM/DAB_TUNE_FE_CFG`/VHFSW switch (0x1712,
  value `0x0001` = closed) all match AN851's "Silicon Labs Recommended Front End
  Network" table exactly (FM: `0xEDB5`/`0x01E3`; DAB: `0xF8A9`/`0x01C6`; switch
  closed on both).
- **FM_TUNE_FREQ command**: all six ARG bytes decoded bit-by-bit against the AN649
  command table (DIR_TUNE, TUNE_MODE, INJECTION, FREQ, ANTCAP, PROG_ID) — correct.
- **INT_CTL_ENABLE / INT_CTL_REPEAT** (STCIEN/STCREP, properties 0x0000/0x0001):
  correct bit positions; confirmed these only gate the physical INTB pin, not the
  STATUS0 STCINT bit our driver polls directly over SPI.
- **STC polling mechanism**: the same raw SPI byte (`pollRx[1]`) that reliably
  reports CTS=1 (bit 7) across hundreds of successful commands also reports
  STCINT=0 (bit 0) — the read path itself is proven reliable by the CTS side, so
  the "never sets" result is a real hardware/firmware-image observation, not a
  polling bug.

## Front-end network component mismatch (found this session, not the cause by itself)

The board's actual front-end network (`RF1 → C13(33pF) → L1(18nH) → C14(2.7pF
shunt) → L3(120nH shunt) → VHFI`, `L2(22nH)` bridging VHFI↔VHFSW) differs from
Silicon Labs' AN851 reference network the VARM/VARB constants were derived from
(`C1=33pF, L1=56nH, L2=120nH‖L3=120nH`). This was flagged as a plausible
contributor, then tested directly and ruled out as the *sole* cause (see below).

## Empirical sweeps (all negative — zero variation)

- **IBIAS/CTUN** (crystal startup calibration, POWER_UP ARG3/ARG8): 8 candidates
  across the practical range, full reboot between each. No change.
- **ANTCAP** (FM_TUNE_FREQ ARG4/5, bypasses FE_VARM/VARB auto-tune entirely and
  forces the on-chip antenna varactor directly, per AN851 Appendix A): ~100 of 128
  possible values swept, across three antenna conditions (disconnected, loose
  contact, directly soldered 70 cm — correct quarter-wave for FM). **Every single
  attempt returned byte-identical RSQ raw data**
  (`00 80 00 00 c0 00 00 00 00 00 00 00`, RSSI=0/SNR=0/VALID=0). If the RF path
  were electrically functional, at least one of ~100 forced varactor values across
  the full physical range should have produced resonance. None did.
- **Reset type**: software RSTB# vs. full USB power-cycle (15 s cold) — no change.
- **PCB continuity**: RF1 (antenna connector) → C13 → L1 → U6 pin 10 (VHFI)
  confirmed intact with a multimeter (tested each leg separately to work around
  C13's DC block). No broken trace/via.

## Leading hypothesis: QFN-48 exposed pad (EP) solder defect

U6 is a 7×7 mm QFN-48 with an exposed thermal/ground pad (pin 49, tied to GND).
Insufficient solder or voiding under this pad during reflow is a well-documented
QFN assembly failure mode that produces exactly this symptom: digital I/O
(peripheral pins, less ground-sensitive) works perfectly, while the RF/analog
front end (which references the exposed pad for a clean ground) fails entirely.
ESD was considered and set aside — the antenna input has ESD clamp protection
(D3, BAV99) ahead of the RF path, and no digital-side symptom consistent with ESD
damage (SPI glitches, crystal instability) has ever appeared.

**Action taken**: a technical report was sent to PCBWay (order W96157ASH49)
requesting an assembly quality review of U6's solder joints, specifically the
exposed pad, laying out the same evidence above (firmware ruled out by the
ANTCAP-bypasses-firmware argument).

**Action pending**: a manual hot-air reflow of U6 (re-melt only, no added
solder/paste) was planned as a lower-risk first attempt before considering full
chip removal and re-paste. Outcome not yet recorded in this document as of this
report's writing — update this section once attempted.

## Separate finding this session: audio profile save was broken, now partially fixed

Independent of the Si4684 investigation, while testing the internet radio
streaming feature (`components/services/webradio`), audio profile changes via
`PUT /api/audio/profile` and `POST /api/audio/reset` were found to fail
(`store_failed`) — which is why the ESP32 mixer channel (needed to hear the web
radio stream through the DSP mixer, muted at -96 dB by the default "radio-first"
mix) could not be un-muted via the API.

Root-caused and fixed: `sigma_i2c_write()` (`components/drivers/adau1701/src/SigmaStudioFW.c`)
had no retry on I2C transaction failure. A full EQ apply chains ~55 sequential
I2C transactions (5 bands × safeload block); a single transient NACK anywhere in
that burst aborted the whole sequence, while short bursts (e.g. the 2-write beep
toggle) reliably succeeded. Added a 3-attempt retry with a 2 ms backoff.

After the fix, DSP-side writes (mixer, EQ) succeed. A **second, separate**
failure remains: `NvsAudioProfileStore::saveProfile()` still fails, now isolated
to the NVS write step itself (not the DSP). Diagnostic logging was added
(`nvs_open`/`nvs_set_str`/`nvs_commit` error codes) to pin down the exact
`esp_err_t`; the leading suspicion is NVS partition space/fragmentation (the
`nvs` partition is only 24 KB, and this session alone did many repeated writes
across streaming config, beep toggles, Wi-Fi, and station data). **Not yet
confirmed with the actual error code — re-run the diagnostic build and capture
the log line to close this out.**

## Blob/firmware-image integrity check (completed, negative — blobs are genuine)

`getPartInfo()` and `getSysState()` (`components/drivers/si4684/src/Si4684Driver.cpp`)
already existed to decode `GET_PART_INFO`/`GET_FUNC_INFO`/`GET_SYS_STATE`, but were
never called anywhere in the codebase — dead code, so their byte-offset bugs had
never surfaced. Found and fixed **two rounds** of off-by-one bugs while wiring them
into a boot-time diagnostic log:

1. First pass: every field (chip ID, firmware major/minor/build, image type) read
   one byte too far right — e.g. `firmwareBuild` was actually reading the
   NOSVN/LOCATION flag byte, not a version number.
2. That "fix" was itself wrong in the other direction. `readRaw()` responses carry
   a one-byte SPI lead-in before STATUS0 — the same convention already confirmed
   and commented in `pollStc()` elsewhere in this file — which the first pass
   didn't account for. Caught empirically: the "fixed" `GET_SYS_STATE` reported
   `image=192` (`0xC0`), the exact byte pattern of STATUS3 with `PUP_STATE=3`
   seen dozens of times elsewhere in this investigation — proof the read was
   still one byte off, in the other direction. All three existing response
   buffer sizes (7/13/24 bytes) already matched "N response bytes + 1 lead-in",
   confirming the lead-in-byte offset (not the no-lead-in offset) is correct.

**Result after the fix**, captured live from the device (DAB boots first at
startup):

```
Si4684: blob streamed: 5796/5796 bytes      (rom_patch_016.bin, matches file size exactly)
Si4684: blob streamed: 517524/517524 bytes  (dab_firmware.bin, matches file size exactly)
Si4684: GET_SYS_STATE: image=2              (2 = DAB active, correct per AN649)
Si4684: GET_PART_INFO/GET_FUNC_INFO: part=4684 rev=4.0.5 svnid=0x00001754
```

`part=4684` matches the expected Si4684 part number exactly; `rev=4.0.5` and the
SVN ID are plausible, sane values, not garbage. Blob byte counts streamed over
SPI match the local file sizes exactly — no truncation in transit. **Verdict:
the DAB blob loaded on the chip is genuine and intact.** (FM boot's GET_FUNC_INFO
was not yet captured — a BT1035 AT-init failure, see below, has been blocking the
device from reaching the point in the boot sequence where FM is exercised. Not
expected to change this verdict; DAB alone already answers the blob-integrity
question this check was for.)

This closes the last plausible firmware-side explanation for the no-lock symptom
**as far as DAB is concerned only** — see the 2026-08-14 update below for a
correction to how far this actually generalizes to FM.

## Open items

1. Confirm the exact NVS error code for the audio-profile save failure and fix
   accordingly (likely: erase/compact the `audio_profile_json` key, or address
   partition fragmentation — do **not** perform a full NVS erase without explicit
   confirmation, it would wipe Wi-Fi credentials, stations, and the saved BT
   speaker pairing).
2. **PCBWay dispute closed (2026-08-14) without resolution.** PCBWay's response:
   AOI passed, no X-ray performed (not requested by their process), and they
   consider the assembly sound unless given photographic evidence — which, per
   the QFN voiding research below, cannot exist for this failure mode by
   construction. The user closed the dispute rather than continue arguing a
   claim neither side can prove without an X-ray neither side is willing/able
   to obtain (single unique board, international shipping not worth the cost
   or risk). **PCBWay is no longer an active avenue.** Remaining options if
   revisited: local X-ray access (university SMT lab, phone-repair/BGA rework
   shop), or the hot-air reflow (still requires the user's explicit go-ahead —
   not to be proposed proactively).
3. Record the hot-air rework outcome (RSSI response test) if/when the user
   decides to attempt it. Not proposed proactively — the user has explicitly
   declined to touch/rework the board without a materially stronger reason
   than what non-invasive diagnostics have produced so far.
4. Visual tilt/float check (2026-08-13/14, two rounds, 8 photos: top-down and
   genuine side-profile/raking-light): **negative** — U6 sits flush, solder
   fillets look even on every edge photographed, no visible gap or lifted
   corner. Rules out the "gross float from paste over-print" QFN failure mode
   specifically (a real, documented failure mode found via web research this
   session). Does **not** rule out sub-visible exposed-pad voiding, which is
   undetectable by any optical method — confirmed via research into QFN
   thermal-pad voiding literature (needs X-ray, see item 2).
5. VA/VCORE analog+core supply rail measured directly at U3 (1.8 V regulator)
   pin 5: **1.791 V**, within the Si4684 datasheet spec (1.71–2.0 V, typ.
   1.8 V). Rail confirmed healthy; this was expected going in, since VA and
   VCORE share the same physical net and VCORE was already known-good (the
   chip boots and answers SPI). Rules out a gross power-rail fault as the
   cause.
6. LO-leakage test (second FM radio near the board while attempting a tune, to
   detect whether the Si4684's local oscillator radiates near the tuned
   frequency + IF) — proposed, **not yet performed/reported** by the user.
   Still the only remaining test that can distinguish "RF synthesizer alive,
   fails downstream" from "RF block itself never starts."

## 2026-08-14 update: FM blob verified, BT1035 root-caused (software, not hardware)

**FM blob integrity — closed.** Live capture, `POST /api/tuner/tune`
`{"band":"fm","frequency_khz":95000}`:

```
Si4684: blob streamed: 531300/531300 bytes   (byte-perfect vs local file)
Si4684: FM firmware booted
Si4684: GET_SYS_STATE: image=1
Si4684: GET_PART_INFO/GET_FUNC_INFO: part=4684 rev=5.1.3 svnid=0x000023b3
```

Genuine, byte-perfect, sane values — same verdict as DAB. Tuning at 101.5 MHz
and 95.0 MHz both reproduce the identical no-lock signature already seen on
DAB (STC timeout, `FM RSQ raw: 00 80 00 00 c0 00 00 00 00 00 00 00`, all
metrics zero).

**New finding**: DAB (rev 4.0.5) and FM (rev 5.1.3) are from two different
Skyworks release generations roughly two years apart (DAB blob sourced from
the PE5PVB community project, FM from a Skyworks eval CD) — confirmed via the
official AN649 Table 1 revision history. Both are individually within
ROM0.016's documented compatibility window, and each band load is an
independent, exclusive HOST_LOAD (never concurrent), so this mismatch is very
unlikely to be functionally relevant. Recorded because it was a real,
previously-unverified gap, not because it changes the verdict.

**This strengthens the hardware hypothesis**: two independently-sourced
firmware images, different vintage, different origin, fail identically. A
shared firmware bug across both is far less plausible than a shared hardware
cause (front-end/EP) that doesn't care which application image is loaded.

**BT1035 `AT init failed` — root-caused and fixed, was software, not hardware.**
Contrary to the working hypothesis from earlier tonight (intermittent physical
contact, correlated with the antenna soldering session), the actual cause was
two regressions introduced by this session's own earlier commit
(`6f7b6dd`), confirmed by diffing against the last commit explicitly logged as
"all companion chips ready" (`6ca40f1`, 2026-08-05):

1. A redundant software `AT+RESET` sent over UART immediately after the
   hardware RESET# pulse — absent from the known-good baseline, which goes
   straight from the hardware pulse into the init handshake. Landing this
   command while the module is still processing the hardware reset risks
   restarting its bring-up mid-sequence.
2. A UART boot-banner diagnostic probe (added earlier this session to
   distinguish "module silent" from "module garbled") with too short a listen
   window (1500 ms) — live capture showed the module's real unsolicited boot
   banner (`+VER=FSC-BT1035,V6.1.1,20240521`) arriving closer to 5 s, well
   outside that window.

Both fixed in `components/drivers/bt1035/src/Bt1035Driver.cpp`: removed the
redundant `AT+RESET`, widened the listen window to 3500 ms. Result: 5/5 clean
boots after the fix, versus roughly 1/13 before. No physical intervention,
cleaning, or component was involved — a visual inspection of the BT1035
module's castellated pads (zero risk, no rework) found nothing abnormal
beyond minor flux residue, consistent with this being a pure software
regression, not a solder defect.

**Net effect on the Si4684 hypothesis**: none directly — BT1035 and Si4684 are
separate chips/subsystems — but it's a useful calibration: a fault that
*looked* exactly like a classic "physical handling damage" symptom (persistent
after a soldering session, deterministic-then-intermittent) turned out to be
100% software. Worth remembering as a caution against over-attributing
intermittent symptoms to hardware without exhausting the code-path diff
against a known-good commit first.

## 2026-08-15 update: front-end network mismatch quantified — does not explain the total blackout

Follow-up on the "Front-end network component mismatch" section above (board
network `C13 33pF, L1 18nH, C14 2.7pF shunt, L3 120nH shunt, L2 22nH` vs
AN851's reference `C1 33pF, L1 56nH, L2‖L3 120nH‖120nH`). The mismatch was
flagged as a plausible contributor but never quantified. Real component
coordinates were pulled directly from `DigiRadio.kicad_pcb` (RF1 at
104.064,92.281; C13 111.811,92.281; L1 112.319,89.868; C14 114.097,91.519; L3
115.621,89.868; L2 115.621,91.9; U6 121.717,90.122 — confirming the network's
physical path and component identity), then modeled as a two-port ABCD chain
(series C13 → series L1 → shunt bank C14‖L3‖L2 at the VHFI node), 50 Ω
reference on both ports. This is a lumped-element approximation: it ignores
PCB trace parasitics, the chip's real complex input impedance at VHFI, and
antenna radiation — good for an order-of-magnitude comparison against the
AN851 reference network, not an absolute number.

(A pure EM/gerber-based simulation via `gerber2ems`/openEMS, initially
considered, was ruled out for this specific question: per its own
documentation, `gerber2ems` does not model discrete capacitors/inductors —
"capacitors are not simulated... they can be approximated by shorting them
using a trace" — which would misrepresent a network that is almost entirely
discrete L/C components.)

**Result** (S21 = insertion loss, S11 = return loss, board network vs AN851
reference):

| Band | Board S21 | Reference S21 | Board S11 | Reference S11 |
|---|---|---|---|---|
| FM 87.5–108 MHz | −7.1 to −9.8 dB | −1.2 to −1.5 dB | −0.5 to −0.9 dB | −5.4 to −6.3 dB |
| DAB 174–240 MHz | −2.4 to −3.4 dB | −2.0 to −3.0 dB | −2.7 to −3.7 dB | −3.1 to −4.4 dB |

**FM**: the board network carries a real 6–9 dB insertion-loss penalty over
the reference network — worth correcting, but not by itself the kind of loss
that silences a strong local FM station on a working receiver (10 dB of
front-end loss is routinely tolerated).

**DAB**: the board network is within ~0.3–1.4 dB of the reference network —
essentially the same insertion loss. The mismatch is not a meaningful factor
at DAB frequencies at all.

**Conclusion**: since DAB shows the identical total-blackout signature as FM
(RSSI/SNR/VALID all zero, unmoved by ~100 ANTCAP sweep values) despite the
front-end mismatch being nearly irrelevant in that band, the network mismatch
cannot be the primary cause of the observed failure on its own. This is a
quantitative point in favor of the existing QFN exposed-pad hypothesis (§
"Leading hypothesis" above), not a competing explanation — it narrows, rather
than replaces, the open items in that section.

## 2026-08-16 update: hot-air reflow attempted — no change to RF symptom

The manual hot-air rework of U6 (re-melt only, no added solder/paste) flagged
as "action pending" in the Leading hypothesis section was carried out: 100°C
for 1 minute, then 220°C for 1.5 minutes, low airflow.

Post-rework, on a fresh build/flash of the current firmware, FM tuning was
retested at three frequencies (100.9, 95.0, 87.9 MHz) via `POST
/api/tuner/tune`. Result: **byte-for-byte identical to every pre-rework
capture in this report.**

```
Si4684: STC timeout: last poll spi_err=0 status=12 c0 00 00 c0 INTB=1
Si4684: FM tune STC timeout at 95000 kHz — settling 150 ms
Si4684: FM RSQ raw: 00 80 00 00 c0 00 00 00 00 00 00 00
Si4684: FM tuned 95000 kHz antcap=0 rssi=0 dBuV snr=0 dB valid=0 readfreq=0
```

Same at 100.9 and 87.9 MHz. RSSI/SNR/VALID all zero, `locked=false`, no
variation from the reflow.

**Item 2/3 (rework outcome) in "Open items" above is now closed: attempted,
no effect.** This does not rule out the QFN exposed-pad hypothesis — a
re-melt without added paste/flux does not reliably resolve a voiding defect
under an exposed pad (only adds heat to already-present solder, doesn't add
volume where a void is) — but it does mean the easy, low-risk fix attempt is
exhausted. Remaining paths are the non-destructive diagnostics proposed this
session (mechanical flex test with live RSSI monitoring, controlled thermal
stress test with live RSSI monitoring, NanoVNA S11 sweep at RF1 chip-on vs
chip-off) or escalating to X-ray/full chip removal, neither attempted yet.

## 2026-08-16 update: root cause found — FM_TUNE_FREQ/DAB_TUNE_FREQ argument-offset bug, not hardware

**This overturns the QFN exposed-pad hypothesis above.** The actual cause of
the months-long "total RF blackout" was a software bug in
`Si4684Driver::tuneFm()`/`tuneDab()`, found by diffing our command
construction against the official AN649 Command 0x30 (FM_TUNE_FREQ) and
Command 0xB0 (DAB_TUNE_FREQ) argument tables directly (page-level read of
`Hardware/DATASHEET/AN649.pdf`, not driver comments), prompted by cross-
referencing against the independent `hitech95/si468x_dab_receiver` Linux
driver.

`Si4684Driver::writeCommand()` always prepends a fixed `ARG1 = 0x00` byte
before whatever payload array is passed to it:

```cpp
buffer[0] = static_cast<std::uint8_t>(cmd);
buffer[1] = 0x00U;                 // ARG1, always
std::memcpy(buffer.data() + 2U, payload, length);   // ARG2 onward
```

`POWER_UP` and `HOST_LOAD` callers already accounted for this correctly
(their arrays are written starting at ARG2). **`tuneFm()` and `tuneDab()`
did not** — both built their argument arrays starting at what the author
believed was ARG1, so every byte actually landed one slot to the right of
where it belongs, with an extra unused byte tacked on the end:

- **FM_TUNE_FREQ** (AN649 Command 0x30): real layout is ARG2=FREQ[7:0],
  ARG3=FREQ[15:8], ARG4=ANTCAP[7:0], ARG5=ANTCAP[15:8], ARG6=PROG_ID. Our
  code sent FREQ's low byte into ARG3 (should be the high byte), the actual
  frequency low byte was always sent as a fixed `0x00`, and the ANTCAP value
  landed in ARG5 (the *high* byte of a 0–128-range field) instead of ARG4.
  **The chip was never told the requested frequency** — it received a
  garbage FREQ value derived from shifted bytes, and the ANTCAP sweep
  documented earlier in this report (~100 values, byte-identical results)
  was sweeping the wrong byte entirely, which is exactly why it never
  produced any variation.
- **DAB_TUNE_FREQ** (AN649 Command 0xB0): same shift. `FREQ_INDEX` (real
  ARG2) was always sent as `0x00`; the actual requested index landed in
  ARG3, which the spec requires to be a fixed `0x00`.

Fixed in `components/drivers/si4684/src/Si4684Driver.cpp`, `tuneFm()` and
`tuneDab()`: removed the extra leading byte and the extra trailing byte so
the arrays start at the real ARG2.

**Result, live on hardware immediately after the fix** (`POST
/api/tuner/tune`, no other change — same antenna, same board, no rework
involved in this result):

```
Si4684: FM RSQ raw: 00 81 80 00 c0 00 02 2e 22 8d fb fd
Si4684: FM tuned 87500 kHz antcap=0 rssi=-5 dBuV snr=-3 dB valid=0 readfreq=87500
```

RSSI/SNR now read real, varying, frequency-dependent values (e.g. −13 to +4
dBuV across a 10-point FM sweep, peaking near a plausible local station at
98.5 MHz) instead of the fixed `00 80 00 00 c0 00 00 00 00 00 00 00` /
all-zero pattern seen in every capture in this report until now. **No STC
timeout occurred in any tune or seek attempt after the fix** — every prior
capture in this document logged one on every single attempt.

`locked`/`valid` is still `false` in this test — expected with the
board's improvised antenna and not yet investigated further; that is now an
ordinary sensitivity/antenna question, not a "chip never responds to RF"
question. DAB was retested at freq_index=10 with no station found
(`fic_quality=0`, `cnr_db=0`) but also with no STC timeout — most likely no
active multiplex at that index/location, to be swept properly with a real
antenna as a follow-up, not evidence against the fix (which addresses the
identical byte-shift bug in both commands).

**What this means for the rest of the investigation**: the QFN exposed-pad
hypothesis, the front-end network mismatch analysis, the hot-air reflow, and
the mechanical flex test were all investigating a symptom that had a
software cause. None of that work was wasted — the empirical rigor (ANTCAP
sweep producing zero variation, DAB and FM failing identically) is exactly
what made this bug's fingerprint recognizable once the actual command bytes
were checked against the primary spec instead of trusted from driver
comments. The lesson: `writeCommand()`'s implicit ARG1 prepend is an easy
trap for future commands — any new caller must remember its array starts at
ARG2, not ARG1.

**Follow-up, completed same session**: audited every remaining
`writeCommand()` call site in `Si4684Driver.cpp` against the AN649 page text
(not driver comments) and found the identical bug pattern repeated in
several more places — `writeCommand()`'s implicit `ARG1=0x00` prepend was
either swallowing a real ARG1 value the caller needed, or shifting a whole
multi-byte struct one slot right:

- **`seekFm()` (FM_SEEK_START, 0x31)**: `SEEKUP`/`WRAP` (real ARG2) were
  never sent — the chip always saw `ARG2=0x00`, so hardware seek always
  searched down with no wrap regardless of what was requested. This is why
  every seek in this report's earlier captures fell through to the
  `hitech95`-inspired 100 kHz software-step fallback in `Si4684Tuner`
  instead of using the chip's real seek.
- **`startDabService()`/`stopDabService()` (0x81/0x82)**: `SERVICE_ID` and
  `COMPONENT_ID` (8 bytes, real ARG4-11) were shifted one byte right into
  ARG5-12, with `SERTYPE` landing in ARG2 (spec: fixed `0x00`) instead of
  ARG1. Playing a specific DAB service would have started the wrong
  service/component or failed outright — not yet observed in practice only
  because tuning itself never worked before this session.
- **`readDabServiceData()` (GET_DIGITAL_SERVICE_DATA, 0x84)**: same
  ARG1-only shift as below, plus the `STATUS_ONLY` bit was coded as `0x08`
  (bit 3) instead of the correct `0x10` (bit 4) per the AN649 bit table.
- **`clearFmStc()`, `readFmRsq()`, `readFmRds()`, `fetchDabServiceList()`,
  `readDabDigRadStatus()`, `readDabEventStatus()`**: all six commands
  (FM_RSQ_STATUS 0x32, FM_RDS_STATUS 0x34, GET_DIGITAL_SERVICE_LIST 0x80,
  DAB_DIGRAD_STATUS 0xB2, DAB_GET_EVENT_STATUS 0xB3) have **only ARG1** in
  the AN649 spec — no ARG2 exists at all. Passing anything through the old
  `writeCommand(cmd, payload, length)` two-argument form for these could
  only ever send a spurious extra byte while the intended ARG1 flag (STCACK,
  INTACK, SERTYPE, DIGRAD ack, EVENT_ACK) silently landed nowhere, since
  `writeCommand()` had no way to set ARG1 to anything but a hardcoded
  `0x00`. `clearFmStc()`'s STCACK never fired in this driver's entire
  history — masked because `FM_TUNE_FREQ`/`FM_SEEK_START` already
  auto-clear STC per their own AN649 documentation.

Fixed by giving `writeCommand()` a fourth parameter, `std::uint8_t arg1 =
0x00U` (default preserves every already-correct call site), and updating
each caller above to either pass its flag byte through `arg1` with no
payload (for the ARG1-only commands) or drop the erroneous leading array
element (for the multi-arg commands whose ARG1 is legitimately always
`0x00`, e.g. `seekFm`'s default tune mode).

**Confirmed live after this round of fixes** — first `locked: true` and
first hardware (non-software-fallback) seek in this entire investigation:

```
POST /api/tuner/tune  {"band":"fm","frequency_khz":87500}
POST /api/tuner/seek  {"direction":"up"}
-> {"frequency_khz":98300}
GET /api/tuner/status
-> {"locked":true,"fm":{"frequency_khz":98300,"rssi_dbuv":12,"snr_db":14,"stereo":false}}
```

The seek jumped directly from 87.5 to 98.3 MHz in one hardware search (not
100 kHz software steps), landing on a real, locked station at a plausible
RSSI/SNR. `bt1035` also came back to `true` in `/api/health` during this
same session (cause not yet diagnosed — see the BT1035 section below;
unrelated to this fix, separate chip).

DAB was swept across 7 frequency-table indices (5, 10, 15, 20, 25, 30, 35)
after the fix — `fic_quality`/`cnr_db` stayed at 0 on all of them, no lock
yet. The DAB_TUNE_FREQ/DAB service-list/service-start fixes are verified
correct against the AN649 spec text the same way the FM fix was, but do not
yet have an empirical lock to point to, unlike FM. Not treated as a red
flag — no DAB antenna tuning has been attempted yet, and the default
European frequency table may not match active local multiplexes at these
particular indices. **Next step**: sweep the full DAB frequency table (not
just 7 samples) with a real antenna and confirm a lock the same way FM was
confirmed.

## 2026-08-16 update: first real audio from Si4684 — ADAU1701 SerialInputRegister IBP polarity

After the FM_TUNE_FREQ/DAB_TUNE_FREQ fix above produced a real lock
(`locked:true`, RSSI +12 dBuV, SNR +13 dB on 98.3 MHz), the speaker was still
silent. This section covers debugging that separate problem — not a Si4684
RF issue, but the digital audio link from Si4684 into the ADAU1701.

**Audit trail, each step verified against a primary source, not assumed:**

1. Re-verified every Si4684 audio property against the AN649 page text:
   `DIGITAL_IO_OUTPUT_SELECT`, `_SAMPLE_RATE`, `_FORMAT` (24-bit sample in
   32-bit I2S slots), `AUDIO_MUTE` (unmuted), `AUDIO_OUTPUT_CONFIG` (was
   incorrectly written with a stray bit — 0x0302's only real field is bit0
   MONO, not an I2S enable; fixed to 0x0000). All correct.
2. `PIN_CONFIG_ENABLE` (0x0800): bit1 I2SOUTEN, bit0 DACOUTEN — AN649 states
   "only I2SOUTEN or DACOUTEN can be enabled at a time; if both enabled, only
   analog output is enabled." The driver was writing `0x0003` (both bits) —
   the chip was falling back to its unused analog DAC output on every boot.
   Fixed to I2SOUTEN-only. Cross-checked against
   `hitech95/si468x_dab_receiver`'s ALSA codec driver
   (`sound/soc/codecs/si468x.c`): their working value is
   `SI468X_PROP_I2S_ENABLED = 0x8002` (I2SOUTEN + INTBOUTEN, bit15) — not
   just `0x0002`. Adopted `0x8002`.
3. Traced the full ADAU1701 SigmaStudio netlist
   (`Firmware/ADAU1701-Firmware/DigiRadio_NetList.xml`, generated export, not
   guessed): Si4674 gain cell → St Mixer1 → PEQ1 → Master gain → Limiter →
   Output, confirmed reachable and correctly addressed (`ADDR_SI4674`/
   `ADDR_SI4674_1` come from the generated `DigiRadio_IC_1_PARAM.h`, not
   hand-typed). Confirmed this whole downstream chain works independently —
   both the Beep1 test tone and the web radio (ESP32) path were audible
   through it before any Si4684 fix.
4. Checked `MpCfg0`/`MpCfg1` (ADAU1701 pin-mux registers, `DigiRadio_IC_1_REG.h`)
   against the ADAU1701 datasheet (`Hardware/DATASHEET/adau1701.pdf`): MP0,
   MP1, MP4, MP5 (SDATA_IN0/1, INPUT_LRCLK, INPUT_BCLK) are correctly
   configured as "Serial data port" function, not left as GPIO.
5. Found a PCB net-name vs. silicon pin-name mismatch while tracing the
   Si4684→ADAU1701 connection in `DigiRadio.kicad_pcb`: Si4684 (U6 pin 33)
   lands on ADAU1701 (U9) physical pin 11, which the datasheet identifies as
   MP0 (silicon channel SDATA_IN1) — the PCB net is *labeled* "SDATA_IN0",
   which does not match the silicon function at that pin. Tested by
   unmuting both the "Si4684" and "ESP32" mixer gain legs simultaneously
   (`Si4674`/`ESP32` cells in the netlist) — this did not by itself fix
   the silence, so the SigmaStudio channel assignment for `Input1` was not
   actually the blocking issue (kept both legs unmuted as a harmless no-op
   change of practice, not reverted).
6. **Root cause**: `SerialInputRegister` (ADAU1701 register 0x081F,
   `Table 49` in the datasheet), which controls the serial input port's
   clock polarities — `ILP` (bit4, LRCLK polarity) and `IBP` (bit3, BCLK
   edge the input data changes/is clocked on). This register is baked into
   the compiled SigmaStudio DSP program export and is not something the
   ESP32 firmware wrote at runtime before now; its compiled value is the
   default `0x00` (ILP=0, IBP=0). Added a diagnostic runtime override in
   `Adau1701Driver::boot()` (via `SIGMA_WRITE_REGISTER_BLOCK`, the same
   primitive the DSP program loader itself uses) to test alternate
   polarities live, without touching the SigmaStudio project:
   - `IBP=1` alone (`0x08`): **real, recognizable music** instead of pure
     static on a locked, strong FM signal — first time ever.
   - `ILP=1` added on top (`0x18`): made it worse (pure white noise again).
   - Back to `IBP=1` alone (`0x08`): music confirmed again, though
     inconsistently — RSSI/SNR fluctuated significantly between otherwise
     identical retunes (SNR seen anywhere from 2 to 14 dB on the same
     station), consistent with a marginal/improvised antenna connection
     rather than a firmware regression. Kept `IBP=1` as the fix.

Fixed in `components/drivers/adau1701/src/Adau1701Driver.cpp`
(`SerialInputRegister` override after DSP program load) and
`components/drivers/si4684/src/Si4684Driver.cpp` (`PIN_CONFIG_ENABLE` =
`0x8002`, `AUDIO_OUTPUT_CONFIG` = `0x0000`).

**Status**: first confirmed end-to-end audio path (Si4684 → ADAU1701 →
BT1035 → Bluetooth speaker) in this project's history. Remaining noise on
top of the music is attributed to antenna quality, not yet independently
confirmed with a proper antenna — flagged as follow-up, not closed.

## 2026-08-16 update: DAB lock confirmed on 3 ensembles + response-offset bug

With the tune fix in place, a full sweep of freq_index 0-35 found three real
ensemble locks (fic_quality=100 on all three): index 5 (CNR 7 dB), index 22
(CNR 15 dB), index 23 (CNR 20 dB, strongest). First confirmed DAB lock in
this project's history.

Chasing why `/api/tuner/services` returned `service_list_empty` even after
30+ seconds on a solid lock found a second bug class, this time in
**response parsing, not command construction**: `readFmRds()`,
`readDabDigRadStatus()`'s `acquired` field, and `readDabEventStatus()` all
read `raw[4]` expecting AN649's "RESP4" field, but this driver's own
established convention elsewhere (`getPartInfo()`, and the already-correct
`ficQuality`/`cnrDb` fields in `readDabDigRadStatus()`) is `raw[5]=RESP4`
(`raw[0]`=SPI lead-in, `raw[1..4]`=STATUS0-3). Fixed all four call sites to
the correct offset; `readFmRds()`'s `fifoUsed`/`blockA-D` fields were
consequently also all off by one and fixed together with it.

After the fix, `serviceListReady` now correctly gates open and
`/api/tuner/services` returns real data instead of `service_list_empty` —
but the entries themselves are still garbled (implausible `service_id`
values, `component_id` fields that decode as ASCII spaces, e.g.
`538976288 = 0x20202020`, mostly-empty labels). This points to a **third,
separate bug** in `fetchDabServiceList()`'s service-list *body* parsing
(the entry structure walked in the loop over `serviceCount`), not yet
investigated — the DAB service list binary format is documented in AN649
§7 "Digital Services User's Guide" (starts around page 418), not the
command tables checked so far. Confirmed live: `POST /api/tuner/play` with
one of these garbled IDs accepted (`{"status":"playing"}`) but produced no
audio, consistent with a wrong service/component ID rather than a new
audio-path regression.

**Follow-up, not done this session**: fix `fetchDabServiceList()` entry
parsing against AN649 §7; then confirm actual DAB audio playback end to
end the same way FM was confirmed.

**Unrelated finding from the same session, logged for completeness**: BT1035
began failing boot deterministically (`no spontaneous UART bytes after
hardware reset`, then `AT init failed`) starting from this session, on both
the firmware build that predates and the one that includes the boot-sequence
fix from `fd9d4ae` — ruling out that fix's absence as the cause. Extending
the diagnostic listen window from 3.5 s to 12 s (temporary, reverted)
produced zero bytes either way, confirming this is not the previously-fixed
"banner arrives late" timing issue but a harder, total UART silence. The
BT1035 module was not physically touched during the U6 rework. Cause not
yet identified; unrelated to the Si4684 investigation (separate chip), but
`HardwareBootstrap::boot()` was changed (`main/hardware_bootstrap.cpp`) to
treat BT1035 boot failure as non-fatal rather than halting the whole device,
so the rest of the system (Si4684 tuning, web UI, Wi-Fi) remains usable
while this is investigated separately.

## 2026-08-19 update: fetchDabServiceList() entry parsing fixed; DAB audio
confirmed, quality traced to signal strength

Live retest on real hardware found `fetchDabServiceList()`'s *body* parsing
(the third bug flagged as "not yet investigated" above) double-counted the
already-consumed SIZE field: it treated the payload as starting 2 bytes
later than it actually does (`serviceCount` read from `body[11]` instead of
`body[9]`, service entries starting at `body[15]` instead of `body[13]`).
AN649 doesn't actually document the DAB service-list entry layout itself —
it defers to a supplemental "Digital Services User's Guide" this project
doesn't have a copy of — so the exact field layout was re-derived by
cross-checking `hitech95/si468x_dab_receiver`'s
`si468x_core_cmd_dab_get_service_list()` (a working Linux driver for the
same command over the same command set), which also confirmed the payload
carried after SIZE is `SIZE-2` bytes, not `SIZE` bytes (fixed the read
sizing to match).

Confirmed live immediately after reflashing: `GET /api/tuner/services` on a
locked DAB ensemble (freq_index 5) now returns 22 real, correctly-decoded
Italian DAB station labels (R.M.T., Radio Cuore, GR News, Radio Sportiva,
Lifegate, ...) instead of an empty list. `POST /api/tuner/play` against one
of these real service/component IDs was confirmed audible — crackly/broken
but present, not silence — on a second try after the first selected
service (R.M.T., `cnr_db=7`) produced no audible sound at all. Switching to
GR News (`cnr_db=8`) did produce audible (if degraded) audio. This matches
DAB's two-tier robustness by design: the FIC channel (`fic_quality` 94-98
throughout) is far more error-protected than the actual audio sub-channel,
so a receiver can report ensemble lock and a clean, complete service list
while individual programme audio is too weak (CNR ~7-8 dB here) to decode
cleanly or at all — the chip's own soft-mute is the most likely explanation
for the first service's total silence, not a firmware defect. This is
consistent with what FM already showed this session ("works, but
badly") and with the still-open antenna/front-end TODO below.

Also found and fixed, unrelated to the Si4684: `SetupWebServer` was
registering 41 HTTP routes against `httpd_config_t::max_uri_handlers = 40`
— `httpd_register_uri_handler()` fails past the limit with only a generic
"no slots left" warning, no indication of which handler was dropped. The
41st and therefore last-registered route, `POST /api/stations/tune`, was
silently unroutable (404) on every boot since whichever commit first pushed
the route count past 40. Bumped to 56 for headroom.

## 2026-08-19 update (2): DAB_EVENT_INTERRUPT_SOURCE never configured;
intermittent multi-second HTTP unresponsiveness noted, still open

Retesting DAB service-list retrieval later the same night found it far less
reliable than the earlier confirmation: `locked:true, fic_quality:97-100`
sometimes took anywhere from ~15s to ~60s to appear after a fresh
`POST /api/tuner/tune` (full Si4684 reboot for the FM->DAB band switch), and
even once locked with excellent FIC quality, `GET /api/tuner/services` kept
returning `service_list_empty` for a further 30-65s.

Root cause candidate found by re-reading AN649's DAB_GET_EVENT_STATUS
section (command 0xB3) carefully: the SVRLISTINT bit this driver polls via
`readDabEventStatus()` is explicitly documented as gated by **Property
0xB300 DAB_EVENT_INTERRUPT_SOURCE**, bit 0 = SRVLIST_INTEN, **default 0x0000
(disabled) at power-on** — and this driver never wrote that property
anywhere. `configureAfterBoot()`'s DAB branch already wrote a
similarly-named `DIGITAL_SERVICE_INT_SOURCE` (property 0x8100), but AN649's
own text for 0x8100 is internally inconsistent between its prose ("configures
digital service interrupt sources") and its bit table (VHFCAPS/VHFSW, a
front-end switch config field) — almost certainly a `pdftotext -raw`
extraction artifact merging two adjacent property tables, the same failure
mode noted earlier this session for AN649/adau1701.pdf text extraction.
0x8100 and 0xB300 are two different properties; only 0xB300's own section
(page ~236) reads internally consistent, so it — not 0x8100 — is the one
that gates SVRLISTINT. Added `setProperty(kPropDabEventIntSource=0xB300,
0x0001)` right after the existing 0x8100 write.

Verified live after reflashing: the service list did come back complete and
correct (all 22 real station labels) on the next test. Not proven
conclusively faster than before — DAB acquisition/list-assembly timing is
inherently variable and this was only tested once post-fix — but the
property write is unambiguously correct per its own AN649 section
regardless, so it stays.

**Separately, and NOT explained by the above**: the HTTP server went fully
unresponsive (connection timeouts on `/api/health`, the simplest possible
route) for 5-10 second stretches, more than once, both before and after
this fix. The heartbeat log line kept appearing on schedule throughout
(`digiradio: heartbeat` every 5s, confirmed via serial), proving the whole
system did not crash or panic — only the HTTP server (or whatever it was
waiting on, most likely a blocking SPI/CTS wait inside the Si4684 driver
triggered from a DAB status/event read) stalled and then recovered on its
own. This was reproducible independent of the 0xB300 change (first
observed hours earlier, unrelated, during the ANTCAP sweep in the antenna
calibration work). Not investigated further tonight — candidate causes to
check next: whether any Si4684Driver SPI wait loop lacks a bound tight
enough for interactive HTTP use, and whether `httpd_config_t::
max_open_sockets = 3` (components/net/src/SetupWebServer.cpp) is simply too
small once anything blocks even briefly.

## TODO (next session)

- **Antenna/front-end calibration — now the real blocker for DAB/FM audio
  quality, not firmware.** Both bands are confirmed working end to end
  (real lock, real service list, real audio) but both are signal-limited:
  FM "works, but badly" per live listening test, and DAB audio ranges from
  crackly to fully soft-muted depending on the service's CNR (~7-8 dB
  observed, on the low side). ~~Redo the ANTCAP sweep~~ — **done this
  session for FM** (see the ANTCAP antenna calibration feature commit);
  antcap=102 saved as the board's default, +6 to +11 dB RSSI/SNR across the
  band. ~~DAB doesn't have an equivalent calibrated-default mechanism yet~~
  — **added and swept 2026-08-20, see below; no default saved (auto-tune
  already best on the ensembles tested).**
- Try a proper FM/DAB antenna to see how much of the crackle/noise clears
  up versus how much is inherent to the current antenna's gain/placement.
- Investigate the intermittent multi-second HTTP unresponsiveness noted
  above — reproducible, not yet root-caused, not obviously related to any
  single change this session.
- BT1035 boot-failure root cause still open (see section above) — non-fatal
  now, so it's no longer blocking, but still unexplained. **Recurred
  2026-08-20, see below — still open, confirmed not caused by physical
  handling.**

## 2026-08-20 update: DAB ANTCAP override added and swept live; BT1035 "total UART silence" recurred

**DAB ANTCAP — implemented, built, flashed, swept live via the HTTP API.**
Extended the ANTCAP override (AN649 Command 0x30 ARG4/5 for FM, Command
0xB0 ARG4/5 for DAB) from FM-only to DAB, mirroring the existing FM
mechanism end to end: `ITuner::tuneDab`/`Si4684Driver::tuneDab` gained an
`antCap` parameter (was hardcoded `0x00`/auto); `TunerService` gained
`defaultDabAntCap_`/`setDefaultDabAntCap()`; `Eeprom24aa` gained
`readDabAntCap()`/`writeDabAntCap()` at word address 0x01 (FM stays at
0x00); `HardwareBootstrap` gained `dabAntCapCalibration()`/
`saveDabAntCapCalibration()`, loaded at boot alongside the FM one; the
`net::AntennaCalibration` bridge gained `saveDab`; both `POST
/api/tuner/tune` (one-shot override, `{"band":"dab","freq_index":N,
"antcap":V}`) and `POST /api/tuner/calibrate-antenna` (persists to EEPROM,
`{"band":"dab","antcap":V}`, `band` defaults to `"fm"` so old clients are
unaffected) now accept DAB. Host build + 20/20 ctest + doxygen +
check-manual-sync all green before flashing.

Swept live via the API (`freq_index` 0-128 step 8) against three real
ensembles:
- **freq_index 5** (weakest known ensemble, 7 dB CNR baseline from the
  2026-08-16 sweep): did not lock at all this session, at any ANTCAP
  including auto — signal currently below threshold, not a code issue
  (indices 22/23 locked normally in the same session).
- **freq_index 23** (strongest, 21-26 dB): CNR jittered ±3 dB across the
  whole ANTCAP range with no discernible trend — already saturated, sweep
  can't discriminate on a signal this strong.
- **freq_index 22** (medium, 16-20 dB): auto (0) and antcap=32 tied for
  best (20 dB CNR); antcap=72 and 80 caused total loss of lock (a dead
  zone to avoid); the rest of the range gave no systematic gain over auto,
  unlike FM's clean +6 to +11 dB improvement.

**Decision: left DAB on auto-tune, nothing saved to EEPROM.** Unlike FM,
no ANTCAP value tested beat the chip's own auto-tune by a margin worth
trusting. If DAB audio quality is still the limiting factor later, retest
specifically on a weak ensemble (index 5 or similar) once it's receivable
again — ANTCAP calibration matters most on weak signals, which is exactly
the case that wasn't testable this session.

**BT1035 "total UART silence" recurred — same still-open issue as before,
confirmed (again) not physical.** During the DAB sweep, the board was
reset several times via opening a `pyserial` connection for log capture —
each open triggers a hardware EN/reset pulse on this ESP32-S3 (confirmed:
happens even with `dsrdtr=False, rtscts=False` and explicit
`setDTR(False)`/`setRTS(False)` — this is the USB-native auto-reset
circuit firing on port open, not a pyserial default that can be disabled
from the Mac side). One of these resets left BT1035 silent: `no
spontaneous UART bytes after hardware reset` on both boot attempts (2/2),
then silent across all 8 probed baud rates (9600-921600). This is *not*
the "banner arrives late" issue fixed 2026-08-20 earlier this same session
(`kBootBannerWaitMs = 25000` was already in effect and made no
difference) — it's the harder, total-silence failure mode already logged
above (the "Unrelated finding from the same session" note before the
2026-08-19 entry), recurring. Confirmed again this time that it is not
caused by physical handling: a full physical power-off for 60 s did not
recover it (Si4684/ADAU1701 both came back up fine on the same power
cycle, ruling out a board-wide power issue). Root cause still not
identified. `/api/bluetooth/status` and `/api/bluetooth/paired` correctly
report `{"status":"error","reason":"at_timeout"}` while in this state; the
rest of the device (tuner, web UI) stays usable per the existing
non-fatal-BT1035-boot design.

## 2026-08-21 update: BT1035 total silence confirmed intermittent (not
## hardware); background retry mitigation added

Follow-up session dedicated entirely to the "total UART silence" BT1035
failure mode above. Summary: **confirmed intermittent on genuinely
identical hardware, root cause narrowed to the module's internal crystal
(not our PCB, not fixable by us), and mitigated (not fixed) with an
indefinite background boot retry.**

**Diagnostic instrumentation (temporary, added then reverted this
session)**: added `BT1035 AT TX: <line>` / `BT1035 UART RX RAW: <hex or
<empty>>` / `BT1035 AT RESULT: OK|ERROR|TIMEOUT` logging around
`Bt1035Driver::transmitAndCollect()`, and temporarily dropped
`kBootAttempts` to 1 for single-attempt clarity. This confirmed the
failure signature precisely: `AT` is transmitted, zero bytes ever come
back (`<empty>`), timeout. Reverted via `git checkout` once the manual
diagnosis was done — not kept in the codebase.

**Multimeter checks, all normal** (scope-level checks — crystal
oscillation, power-on transient — remain out of reach without an
oscilloscope):
- VBAT_IN: 3.3V (datasheet range 3.0-4.2V) ✓
- 1.8V_OUT (module's internal regulator): 1.8V ✓ — proves the module's
  own power management *is* running, it isn't simply unpowered
- SYS_CTRL / RESET (post-boot): ~3.27V, matching the firmware's own GPIO
  readback log (`post-reset: SYS_CTRL=1 RESET=1`) ✓
- BT1035 TX pin (module side) to GND: 3.29V, idle-HIGH, no short/float/
  reversed polarity ✓ (though idle-HIGH alone doesn't prove the module's
  firmware is executing — some pads default HIGH from reset state alone)
- A 10kΩ pull-down the user had added on SYS_CTRL (matching the
  datasheet's own recommendation for an undriven pin) was checked and is
  not the cause — the ESP32 GPIO drives push-pull and its own readback
  confirms it reaches a valid HIGH regardless.

**Crystal location determined**: the BT1035 datasheet's own block diagram
shows "32MHz Crystal" as an internal block of the QCC3056 die, and the
DigiRadio schematic netlist (`Netlist_Schematic1_2026-08-07.asc`) has no
XTAL_IN/XTAL_OUT pins wired to any external crystal for U11 — confirming
the oscillator is sealed inside the Feasycom module, not on our PCB. This
is why nothing on our side (layout, load caps, our firmware) can affect
it; if the failure really is a marginal oscillator-startup margin, it's a
property of that specific physical module unit (or the part's design
tolerance in general).

**Decisive evidence of intermittency, not a dead unit**: across repeated
reboots in the same session (physical power-cycles and serial-port-open
resets, which also hard-reset this ESP32-S3's native USB-CDC), the
identical physical module was observed to boot **completely successfully**
at least once — spontaneous banner `+VER=FSC-BT1035,V6.1.1,20240521` +
`+DEVSTAT=1`, then `AT` and `AT+AUXCFG=3` both answered `OK` — and to fail
completely silently on other attempts, with no physical change in between.
This rules out "defective/dead module" as an explanation; ordering a
replacement module is therefore *not* a guaranteed fix, since the same
physical unit demonstrably works when it works.

**UART loopback test attempt — inconclusive, logged for future
reference.** Tried to isolate ESP32 vs. module by bridging the ESP32-S3's
own GPIO40 (BT1035 UART TX)/GPIO41 (BT1035 UART RX) pins with a jumper
held by hand on the ESP32 module's castellated pads (no series
resistor/test point exists on this net per the schematic netlist — U8.33
↔ U11.P$14 and U8.34 ↔ U11.P$13 directly, nothing else). Twice
reproducibly, bridging those pins from cold boot caused the ESP32 itself
to hang very early in boot (right after the bootloader's "Disabling RNG
early entropy source" line, before `app_main()` even starts) — harmless
(board recovers fully once the jumper is removed) but unexplained, and it
sidesteps the actual test rather than answering it. Not pursued further
this session given the practical difficulty of hand-holding a wire onto
castellated pads without a proper SMD test hook. If retried: attach the
jumper *after* the ESP32 has already booted past that early stage (there's
a ~25s window before the BT1035 AT command is actually sent) rather than
from a cold boot.

**Mitigation implemented: indefinite background boot retry.** Since the
module's own internal fault (if that's what it is) isn't something we can
fix, and since it demonstrably self-clears on a later attempt rather than
needing repair, `main/hardware_bootstrap.cpp` now spawns a
`bt1035RetryTask` FreeRTOS task whenever the initial `HardwareBootstrap::
boot()`'s call to `Bt1035Driver::boot()` fails. The task loops calling
`boot()` again with **no artificial delay** between attempts — each
attempt already blocks for ~25-60s on its own (the banner wait times
`kBootAttempts`, plus an 8-step baud-rate sweep on final failure), so no
extra backoff is needed on top — until it succeeds, at which point it runs
the same post-boot setup (device name, auto-reconnect) the normal success
path does, then exits. The rest of the system (Wi-Fi, tuner, web UI) never
blocks on this and stays fully usable throughout. Verified live: after a
forced failure (2 attempts + baud sweep, ~62s), the retry task started
immediately, the HTTP server and heartbeat came up normally in parallel,
and the retry task began a fresh attempt right away without any pause.

**Open going forward**: root cause of the intermittent total-silence mode
is still not identified — this session's diagnosis exhausted what's
possible with a multimeter alone. Real progress would need either an
oscilloscope on SYS_CTRL/RESET/crystal across several boots to correlate
success/failure with power-on timing jitter, or a large-N automated
reboot-cycle statistic (attempted this session via a `pyserial` script,
but the ESP32-S3's native USB-CDC re-enumerating on every hardware reset
made a fully unattended multi-cycle script unreliable — a naive read loop
silently produced a false "0/5 success" result once across a reconnect
window). A future attempt at that statistic needs to detect the USB path
disappearing/reappearing and reopen the port, or use a separate
hardware UART-to-USB adapter that doesn't disconnect when the target
resets.

**Also discussed this session (not implemented, for a future hardware
revision)**: whether a different/newer SoC could eliminate the need for
the external BT1035 module entirely. Confirmed via web search that
Espressif's new **ESP32-S31** (RISC-V, announced April 2026) has
integrated **Bluetooth 5.4 with both LE and Classic (BR/EDR)** support —
unlike the ESP32-S3 used today, which is BLE-only at the silicon level
(confirmed: no Classic BT/A2DP hardware exists on S3, this is not a
firmware limitation). An `ESP32-S31-WROOM-3` module also exists. This
would be a significant main-MCU redesign, not a drop-in swap, and its
ESP-IDF support maturity/availability wasn't independently verified this
session — worth a dedicated evaluation before committing to it for a
future hardware revision.

## 2026-08-21 follow-up: git archaeology on the boot-retry structure;
## minimal patch to restore the validated single-attempt design

Separate follow-up session, requested specifically to re-derive the
BT1035 boot regression analysis directly from git history rather than
from further live hardware probing, per the project's own house rule
(2026-08-14 postmortem): exhaust the code-path diff against a known-good
commit before floating new hardware theories.

**Full commit archaeology** (`git log --follow` on
`Bt1035Driver.cpp`):
```
6ca40f1 "all companion chips ready" — baseline, 0 known bugs
6f7b6dd added a redundant AT+RESET right after the hardware reset pulse
fd9d4ae (2026-08-15) fixed 6f7b6dd in one commit: removed the redundant
        AT+RESET AND introduced logRawUartBoot() for the first time,
        already at its final 3500ms window (the "1500ms too short"
        text in the report/commit message describes an intermediate
        value tried live during that debugging session, never itself
        committed) — 5/5 clean boots documented after this fix.
3a58d33 (2026-08-20, this project's own earlier commit today) widened
        the banner wait 3500ms → 25000ms (real banner measured arriving
        up to ~18.5-42s post-reset) AND, in the same commit, introduced
        a NEW intra-boot() retry loop (kBootAttempts=2, only
        kBootRetryDelayMs=300ms between the two hardware reset pulses)
        that did not exist in fd9d4ae's validated design.
```

**Finding**: comparing `fd9d4ae` (the last commit with a documented,
validated 5/5 clean-boot run) against the working tree confirmed exactly
three differences, only one of them structural:
1. Banner wait 3500ms → 25000ms — justified by this session's own real
   measurements, kept.
2. `GPIO_MODE_OUTPUT` → `GPIO_MODE_INPUT_OUTPUT` on RESET/SYS_CTRL —
   purely additive (enables `gpio_get_level()` readback for the
   pre-power/post-syscl/post-reset diagnostic logs), electrically
   neutral, kept.
3. **A new intra-`boot()` retry loop with only 300ms between the two
   hardware reset pulses — this did not exist in the validated baseline.**
   The BT1035 datasheet's own "Reset Protection timeout (typically
   >1.8s)" (already gathered earlier this session) means a second
   SYS_CTRL/RESET pulse fired only 300ms after a failed attempt would not
   reliably reach a clean power-off state — risking re-interrupting the
   module mid bring-up, the same class of bug 6f7b6dd/fd9d4ae already
   dealt with once (redundant AT+RESET). This is the only difference
   flagged as a plausible contributor, not asserted as certain.

Also confirmed via repo-wide search: `AT+RESET` (`Bt1035AtCommand::Reset`)
is referenced only in the unit test, never in production code; no other
task/thread touches the BT1035 UART during its boot window
(`savedSpeakerReconnectTask` only starts after `HardwareBootstrap::boot()`
returns; the new `bt1035RetryTask` calls `boot()` sequentially, never
concurrently). `logRawUartBoot()`'s single `uart_read_bytes()` call and
the following `uart_flush_input()` were confirmed, both by code reading
and by this session's own successful-boot log capture (banner appeared,
then `AT`→`OK` immediately after, no stall), to not swallow or discard
data that `runInitSequence()` would otherwise need — `runInitSequence()`
does its own fresh TX/RX cycle regardless of what the banner-capture step
saw.

**Minimal patch applied** (user-directed, exact scope agreed before
touching code): removed the intra-`boot()` retry loop entirely —
`boot()` now makes exactly one `resetAndInitOnce()` call per invocation,
structurally identical to `fd9d4ae`. Removed `kBootAttempts` and
`kBootRetryDelayMs` (dead after the loop's removal); `probeBaudRates()`'s
log line adjusted accordingly (no longer references the removed
attempt count). `kBootBannerWaitMs=25000` and the `GPIO_MODE_INPUT_OUTPUT`
readback were explicitly left untouched. Retries now live exclusively one
layer up, in `hardware::bt1035RetryTask` (`main/hardware_bootstrap.cpp`,
added earlier this session), which only re-invokes a full, clean `boot()`
call — never re-pulses the pins faster than one whole boot cycle apart.
Host tests (20/20) and firmware build both green before flashing.

**Live result after flashing**: structurally the retry cadence is now
clean — confirmed via serial log, each `bt1035RetryTask` iteration is
spaced ~31.8s apart (25s banner wait + ~2s AT timeout + ~4s baud sweep,
no extra gap), matching the intended single-attempt-per-call design
exactly, versus the old back-to-back double-pulse. **However, a 20-minute
monitoring window immediately after flashing captured 31 consecutive
retry attempts, all silent — zero successes**, a worse hit rate in this
specific sample than earlier in the day (which had at least one clean
success among fewer attempts). This neither confirms nor refutes the
Reset-Protection-timing hypothesis on its own — the patch is kept because
it's structurally correct (matches the one historically validated design,
removes the only unexplained difference from it), not because this
sample proves it improved the success rate. The underlying intermittent
root cause (most likely the module's internal, sealed 32MHz crystal
startup margin — see the 2026-08-21 entry above) remains unresolved and
would need an oscilloscope to pin down further.
