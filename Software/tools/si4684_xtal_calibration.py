#!/usr/bin/env python3
"""si4684_xtal_calibration.py — live Si4684 crystal trim via FM_RSQ FREQOFF.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

Uses the Si4684's own FM_RSQ_STATUS FREQOFF field (AN649 Command 0x32,
RESP8, signed offset in units of 2 PPM) as a precision frequency reference
-- real FM broadcast transmitters are GPS/rubidium-locked, so a locked
station's carrier reads the same PPM error as the receiver's own crystal
reference error, no lab equipment required.

Procedure (matches AN649 §9.3's own recommendation to trim by measurement,
just automated over HTTP instead of by ear):

  1. Tune to a strong, stable FM station.
  2. Read GET /api/tuner/status -> fm.freqoff_ppm.
  3. new_xtal_freq_hz = 19200000 * (1 + ppm/1e6)
  4. POST /api/tuner/xtal-calibrate {"xtal_freq_hz": new_xtal_freq_hz}
     (this reboots the Si4684, no ESP32 restart, no persistence yet).
  5. Re-tune, re-read freqoff_ppm. Repeat until it converges near 0.
  6. Optionally cross-check on a second station at the other end of the
     band -- if the residual offset differs systematically between the two,
     the error isn't (only) the crystal; don't chase it further with this
     tool.

Nothing here is persisted to flash. Once you have a converged xtal_freq_hz,
hand it to a human/Claude to bake into the Si4684Driver::boot() default
call in main/hardware_bootstrap.cpp -- this script only calibrates the
currently-running session.

Usage:
  python3 tools/si4684_xtal_calibration.py --host 192.168.1.62 \\
      --frequency-khz 87600

  python3 tools/si4684_xtal_calibration.py --host 192.168.1.62 \\
      --frequency-khz 87600 --once   # single read, no correction loop
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.request
from typing import Optional


def http_request(host: str, method: str, path: str, payload: Optional[dict],
                  timeout: float) -> dict:
    url = f"http://{host}{path}"
    # Compact separators: the firmware's hand-rolled JSON parser is not
    # whitespace-tolerant (confirmed bug in components/core/src/TunerJson.cpp).
    data = json.dumps(payload, separators=(",", ":")).encode() if payload is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read()
        return json.loads(body) if body else {}


def tune_fm(host: str, frequency_khz: int, timeout: float) -> dict:
    return http_request(host, "POST", "/api/tuner/tune",
                        {"band": "fm", "frequency_khz": frequency_khz}, timeout)


def read_status(host: str, timeout: float) -> dict:
    return http_request(host, "GET", "/api/tuner/status", None, timeout)


def recalibrate(host: str, xtal_freq_hz: int, ibias: int, ctun: int,
                timeout: float) -> dict:
    return http_request(
        host, "POST", "/api/tuner/xtal-calibrate",
        {"xtal_freq_hz": xtal_freq_hz, "ibias": ibias, "ctun": ctun}, timeout)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Trim Si4684 XTAL_FREQ live using FM_RSQ_STATUS FREQOFF "
        "-- no lab equipment, uses locked broadcast carriers as reference.")
    parser.add_argument("--host", required=True, help="Device IP or mDNS host")
    parser.add_argument("--frequency-khz", type=int, required=True,
                        help="A strong, stable FM station to lock onto, e.g. 87600")
    parser.add_argument("--ibias", type=int, default=72, help="POWER_UP IBIAS (0-127)")
    parser.add_argument("--ctun", type=int, default=0,
                        help="POWER_UP CTUN (0-63) -- default 0, the best value "
                        "found by ear on 2026-08-23; leave alone unless you have "
                        "a reason to also move it")
    parser.add_argument("--start-xtal-freq-hz", type=int, default=19200000,
                        help="Starting XTAL_FREQ before the first correction")
    parser.add_argument("--max-iterations", type=int, default=6)
    parser.add_argument("--converge-ppm", type=float, default=1.0,
                        help="Stop once |freqoff_ppm| is under this many ppm")
    parser.add_argument("--settle-s", type=float, default=1.0,
                        help="Delay after tune/recalibrate before reading status")
    parser.add_argument("--samples", type=int, default=5,
                        help="FREQOFF reads averaged per iteration (reduces "
                        "reception-noise jitter in the ppm estimate)")
    parser.add_argument("--sample-gap-s", type=float, default=0.3)
    parser.add_argument("--damping", type=float, default=0.6,
                        help="Fraction of the measured ppm correction applied "
                        "per iteration (<1.0 avoids overshoot/oscillation "
                        "around the true value)")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--once", action="store_true",
                        help="Single read only, no correction loop")
    args = parser.parse_args()

    xtal_freq_hz = args.start_xtal_freq_hz

    for iteration in range(1 if args.once else args.max_iterations):
        try:
            recalibrate(args.host, xtal_freq_hz, args.ibias, args.ctun, args.timeout)
        except (urllib.error.URLError, OSError) as exc:
            print(f"error: xtal-calibrate failed: {exc}", file=sys.stderr)
            return 1
        time.sleep(args.settle_s)

        try:
            status = tune_fm(args.host, args.frequency_khz, args.timeout)
        except (urllib.error.URLError, OSError) as exc:
            print(f"error: tune failed: {exc}", file=sys.stderr)
            return 1
        time.sleep(args.settle_s)

        samples = []
        for s in range(args.samples):
            if s > 0:
                time.sleep(args.sample_gap_s)
            try:
                status = read_status(args.host, args.timeout)
            except (urllib.error.URLError, OSError) as exc:
                print(f"error: status read failed: {exc}", file=sys.stderr)
                return 1
            fm = status.get("fm") or {}
            if status.get("locked", False) and fm.get("freqoff_ppm") is not None:
                samples.append((fm["freqoff_ppm"], fm.get("rssi_dbuv"), fm.get("snr_db")))

        if not samples:
            print("  no lock / no FREQOFF reading across all samples -- pick "
                  "a stronger station and retry", file=sys.stderr)
            return 1

        ppm_avg = sum(s[0] for s in samples) / len(samples)
        rssi = samples[-1][1]
        snr = samples[-1][2]
        print(f"iter {iteration}: xtal_freq_hz={xtal_freq_hz}  "
              f"samples={len(samples)}/{args.samples}  rssi={rssi}  snr={snr}  "
              f"freqoff_ppm_avg={ppm_avg:.1f}  "
              f"raw={[s[0] for s in samples]}")

        if args.once:
            return 0

        if abs(ppm_avg) <= args.converge_ppm:
            print(f"\nConverged: xtal_freq_hz={xtal_freq_hz} "
                  f"(residual {ppm_avg:.1f} ppm avg, ibias={args.ibias}, "
                  f"ctun={args.ctun})")
            print("Not persisted -- to make this the boot default, edit the "
                  "gSi4684.boot(...) call in main/hardware_bootstrap.cpp.")
            return 0

        # Empirically determined 2026-08-23: correcting in the "+ppm"
        # direction diverges (each iteration made freqoff_ppm larger, not
        # smaller). The correct sign is "-ppm" -- confirmed by a live A/B
        # test, not derived from AN649 (which doesn't specify the relation
        # between XTAL_FREQ and FREQOFF's sign convention). Damping avoids
        # overshoot from single-reading reception noise.
        xtal_freq_hz = round(xtal_freq_hz * (1.0 - args.damping * ppm_avg / 1e6))

    print(f"\nDid not converge within {args.max_iterations} iterations "
          f"(last xtal_freq_hz={xtal_freq_hz}). Try again, or check the "
          f"second-station cross-check described in this script's docstring "
          f"-- a residual that varies with frequency isn't the crystal.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
