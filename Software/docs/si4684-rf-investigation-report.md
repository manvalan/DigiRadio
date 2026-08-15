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
