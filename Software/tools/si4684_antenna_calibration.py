#!/usr/bin/env python3
"""si4684_antenna_calibration.py — AN851 Appendix A varactor-tuning sweep.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

Runs the Si4684 antenna varactor calibration procedure described in AN851
("Si468x AM/AMHD-FM/FMHD-DAB/DAB+ antenna/matching network design
guidelines"), Appendix A, driving the device's existing HTTP tuner API
instead of a bench Test_Get_RSSI command:

  1. At each of several test frequencies, sweep ANTCAP from --antcap-min
     to --antcap-max (AN851: 1-128) via POST /api/tuner/tune, averaging
     --samples (AN851: 5) reads per step via GET /api/tuner/status.
  2. Pick the ANTCAP with the best average reading at each frequency.
  3. Linear-fit best_antcap(frequency_MHz) = (m/1000)*frequency_MHz + b,
     AN851's own formula, giving VARM (m, property 0x1710) and VARB
     (b, property 0x1711) as signed 16-bit integers.

Scope and limits (read before running):

* This firmware's HTTP API has no endpoint to write VARM/VARB directly
  (see components/net/src/SetupWebServer.cpp) -- only a single fixed
  ANTCAP override can be persisted, via POST /api/tuner/calibrate-antenna.
  This script only COMPUTES the fit and prints it; it does not write
  anything to the device. Applying the result means hand-editing the
  kFmTuneFeVarm/kFmTuneFeVarb (or the DAB kDabProps table) constants in
  components/drivers/si4684/src/Si4684Driver.cpp and reflashing.

* DAB caveat: DAB_TUNE_FREQ (AN649 Command 0xB0) addresses frequencies by
  an index into a chip-side table (default "European frequency list")
  loaded via DAB_SET_FREQ_LIST. AN649's copy in this repo does not publish
  that table's contents, and neither the firmware nor its HTTP API expose
  the real MHz for a given freq_index. This script does NOT assume any
  index<->MHz mapping -- for --band dab you must supply the real
  frequency for each index yourself (e.g. read off a known ensemble's
  published transmitter frequency). Do not guess; a wrong MHz value here
  silently produces a wrong VARM/VARB fit.

* DAB metric caveat: AN851 Appendix A calls for RSSI at each step, but
  this firmware's DAB status JSON does not expose RSSI (only
  dab.fic_quality and dab.cnr_db -- see core/TunerStatus.hpp). This
  script uses dab.cnr_db as the optimization metric for --band dab
  instead. That is a deliberate substitution, not a datasheet value --
  treat DAB fit results with more skepticism than FM ones.

* Known dead zone (docs/si4684-rf-investigation-report.md, 2026-08-20):
  DAB freq_index 22 lost lock entirely at antcap 72 and 80 on this board.
  This script does not skip any antcap value in range; a step with no
  lock is logged as "NO LOCK" and simply excluded from that point's
  average, not treated as a fatal error.

Usage:
  python3 tools/si4684_antenna_calibration.py --host digiradio-XXXXXX.local \\
      --band fm --point 88500 --point 98000 --point 107900

  python3 tools/si4684_antenna_calibration.py --host 192.168.1.56 \\
      --band dab --point 5:174928 --point 12:198160 --point 23:225648 \\
      --raw-csv dab_sweep.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import socket
import statistics
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Optional


def parse_point(band: str, raw: str) -> dict:
    """Parse one --point argument into a tune target for the given band."""
    if band == "dab":
        if ":" not in raw:
            raise ValueError(
                "DAB points must be 'freq_index:frequency_khz', e.g. 12:198160"
            )
        idx_s, khz_s = raw.split(":", 1)
        idx = int(idx_s)
        khz = int(khz_s)
        if not 0 <= idx <= 37:
            raise ValueError("freq_index must be 0-37 (core/TunerJson.cpp limit)")
        if khz <= 0:
            raise ValueError("frequency_khz must be positive")
        return {
            "index": idx,
            "khz": khz,
            "mhz": khz / 1000.0,
            "label": f"idx{idx}@{khz / 1000:.3f}MHz",
        }
    khz = int(raw)
    if not 87500 <= khz <= 108000:
        raise ValueError("FM frequency_khz should be within 87500-108000")
    return {"khz": khz, "mhz": khz / 1000.0, "label": f"{khz / 1000:.3f}MHz"}


def tune_payload(band: str, point: dict, antcap: int) -> dict:
    if band == "dab":
        return {"band": "dab", "freq_index": point["index"], "antcap": antcap}
    return {"band": "fm", "frequency_khz": point["khz"], "antcap": antcap}


def extract_metric(status: dict, band: str) -> Optional[float]:
    """AN851's optimization metric: RSSI for FM, CNR for DAB (see docstring)."""
    if not status.get("locked", False):
        return None
    if band == "fm":
        fm = status.get("fm") or {}
        rssi = fm.get("rssi_dbuv")
        return float(rssi) if rssi is not None else None
    dab = status.get("dab") or {}
    cnr = dab.get("cnr_db")
    return float(cnr) if cnr is not None else None


def http_request(
    host: str,
    method: str,
    path: str,
    payload: Optional[dict],
    timeout: float,
    retries: int,
) -> dict:
    url = f"http://{host}{path}"
    # The firmware's JSON parser (components/core/src/TunerJson.cpp) does
    # plain substring search for e.g. `"key":` and is not whitespace-
    # tolerant -- json.dumps()'s default ": "/", " separators break every
    # request with "invalid_json". Compact separators match what curl's
    # -d with no spaces sends, which the firmware does accept.
    data = json.dumps(payload, separators=(",", ":")).encode("utf-8") if payload is not None else None
    headers = {"Content-Type": "application/json"} if data is not None else {}
    last_err: Optional[BaseException] = None
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, data=data, method=method, headers=headers)
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                body = resp.read()
                return json.loads(body) if body else {}
        except (urllib.error.URLError, socket.timeout, TimeoutError) as exc:
            last_err = exc
            if attempt < retries:
                time.sleep(1.0 + attempt)  # device is known to stall briefly under HTTP load
    raise RuntimeError(f"{method} {path} failed after {retries + 1} attempt(s): {last_err}")


def sweep_point(
    host: str,
    band: str,
    point: dict,
    antcap_values: list,
    samples: int,
    settle_s: float,
    sample_gap_s: float,
    timeout: float,
    retries: int,
    raw_rows: Optional[list],
) -> list:
    results = []
    for antcap in antcap_values:
        try:
            status = http_request(
                host, "POST", "/api/tuner/tune", tune_payload(band, point, antcap), timeout, retries
            )
        except RuntimeError as exc:
            print(f"  antcap={antcap:3d}  tune failed: {exc}", file=sys.stderr)
            results.append((antcap, None))
            continue

        time.sleep(settle_s)
        readings = []
        first = extract_metric(status, band)
        if first is not None:
            readings.append(first)
        for _ in range(max(0, samples - 1)):
            time.sleep(sample_gap_s)
            try:
                status = http_request(host, "GET", "/api/tuner/status", None, timeout, retries)
            except RuntimeError as exc:
                print(f"  antcap={antcap:3d}  status read failed: {exc}", file=sys.stderr)
                continue
            reading = extract_metric(status, band)
            if reading is not None:
                readings.append(reading)

        if raw_rows is not None:
            for i, reading in enumerate(readings):
                raw_rows.append(
                    {"point": point["label"], "antcap": antcap, "sample": i, "metric": reading}
                )

        avg = statistics.mean(readings) if readings else None
        state = "locked" if readings else "NO LOCK"
        avg_str = f"{avg:6.2f}" if avg is not None else "   -  "
        print(f"  antcap={antcap:3d}  samples={len(readings)}/{samples}  avg={avg_str}  [{state}]")
        results.append((antcap, avg))
    return results


def linear_fit(xs: list, ys: list) -> Optional[tuple]:
    n = len(xs)
    if n < 2:
        return None
    mean_x = sum(xs) / n
    mean_y = sum(ys) / n
    num = sum((x - mean_x) * (y - mean_y) for x, y in zip(xs, ys))
    den = sum((x - mean_x) ** 2 for x in xs)
    if den == 0:
        return None
    slope = num / den
    intercept = mean_y - slope * mean_x
    return slope, intercept


def main() -> int:
    parser = argparse.ArgumentParser(
        description="AN851 Appendix A ANTCAP sweep + VARM/VARB linear fit over the tuner HTTP API.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--host", required=True, help="Device IP or mDNS host, e.g. digiradio-XXXXXX.local")
    parser.add_argument("--band", required=True, choices=["fm", "dab"])
    parser.add_argument(
        "--point",
        action="append",
        required=True,
        metavar="FREQ_KHZ|INDEX:FREQ_KHZ",
        help="FM: frequency in kHz (e.g. 98000). DAB: freq_index:frequency_khz "
        "(e.g. 12:198160) -- see script docstring for why the MHz value must "
        "come from you, not this script.",
    )
    parser.add_argument("--antcap-min", type=int, default=1)
    parser.add_argument("--antcap-max", type=int, default=128)
    parser.add_argument("--antcap-step", type=int, default=1)
    parser.add_argument(
        "--samples", type=int, default=5, help="Reads averaged per ANTCAP step (AN851 Appendix A: 5)"
    )
    parser.add_argument(
        "--settle-ms",
        type=int,
        default=150,
        help="Delay after tune before the first reading (engineering margin, not from AN851)",
    )
    parser.add_argument("--sample-gap-ms", type=int, default=80, help="Delay between repeated status reads")
    parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout per request, seconds")
    parser.add_argument(
        "--retries", type=int, default=2, help="Retries per request (device is known to stall briefly)"
    )
    parser.add_argument("--raw-csv", type=Path, default=None, help="Optional path to dump every raw sample")
    args = parser.parse_args()

    if not 1 <= args.antcap_min <= args.antcap_max <= 128:
        print("error: antcap range must be within 1-128 (0 = auto, not part of the sweep)", file=sys.stderr)
        return 1

    points = []
    for raw in args.point:
        try:
            points.append(parse_point(args.band, raw))
        except ValueError as exc:
            print(f"error: invalid --point {raw!r}: {exc}", file=sys.stderr)
            return 1

    antcap_values = list(range(args.antcap_min, args.antcap_max + 1, args.antcap_step))
    raw_rows: Optional[list] = [] if args.raw_csv else None

    fit_points = []
    for point in points:
        print(f"\n=== sweeping {args.band} @ {point['label']} ===")
        results = sweep_point(
            args.host,
            args.band,
            point,
            antcap_values,
            args.samples,
            args.settle_ms / 1000.0,
            args.sample_gap_ms / 1000.0,
            args.timeout,
            args.retries,
            raw_rows,
        )
        valid = [(a, v) for a, v in results if v is not None]
        if not valid:
            print(f"  WARNING: no locked reading anywhere -- excluding {point['label']} from the fit")
            continue
        best_antcap, best_val = max(valid, key=lambda t: t[1])
        print(f"  best: antcap={best_antcap} ({best_val:.2f})")
        fit_points.append((point["mhz"], best_antcap, point["label"]))

    if args.raw_csv:
        with args.raw_csv.open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=["point", "antcap", "sample", "metric"])
            writer.writeheader()
            writer.writerows(raw_rows)
        print(f"\nraw samples written to {args.raw_csv}")

    print(f"\n=== AN851 Appendix A fit (band={args.band}) ===")
    if len(fit_points) < 2:
        print(
            "Not enough locked points to fit VARM/VARB (need >= 2 usable frequencies). "
            "Try different --point values or check the antenna/signal."
        )
        return 1

    xs = [p[0] for p in fit_points]
    ys = [p[1] for p in fit_points]
    fit = linear_fit(xs, ys)
    if fit is None:
        print("Fit failed (degenerate frequency set -- all points at the same MHz?).")
        return 1

    slope, intercept = fit
    m = round(slope * 1000)
    b = round(intercept)
    m_ok = -32768 <= m <= 32767
    b_ok = -32768 <= b <= 32767

    print("  points used: " + ", ".join(f"{label} -> antcap {a}" for _, a, label in fit_points))
    print("  varactor_value = (m/1000)*frequency_MHz + b   [AN851 Appendix A]")
    print(f"  m (slope x1000, property 0x1710) = {m}" + ("" if m_ok else "  ** OUT OF int16 RANGE **"))
    print(f"  b (intercept,   property 0x1711) = {b}" + ("" if b_ok else "  ** OUT OF int16 RANGE **"))
    if m_ok and b_ok:
        print(f"  -> 0x1710 = 0x{m & 0xFFFF:04X}   0x1711 = 0x{b & 0xFFFF:04X}")
    print()
    print("  Not written to the device. To apply: edit the k{Fm,Dab}TuneFeVarm/")
    print("  k{Fm,Dab}TuneFeVarb constants in components/drivers/si4684/src/Si4684Driver.cpp")
    print("  (current values and AN851 Appendix B citation are next to them), reflash, then")
    print("  re-run this sweep with antcap=0 (auto) at the same points to confirm auto-tune")
    print("  now tracks the measured optimum.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
