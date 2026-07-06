#!/usr/bin/env python3
"""extract_si4684_blob.py — convert Si4684 C arrays or copies to .bin files.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

Usage:
    # From a SigmaStudio-style C header (PE5PVB firmware.h, Si468xROM.h):
    tools/extract_si4684_blob.py --header src/firmware.h --array firmware \\
        --out Firmware/Si4684-Firmware/dab_firmware.bin

    # Copy an existing Skyworks .bin from an eval-board folder:
    tools/extract_si4684_blob.py --copy ~/si46xx_firmware/fm_radio_5_1_0.bin \\
        --out Firmware/Si4684-Firmware/fm_firmware.bin
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path


def extract_c_array(header: Path, array_name: str) -> bytes:
    text = header.read_text(encoding="utf-8", errors="replace")
    pattern = rf"{re.escape(array_name)}\s*\[\s*\]\s*=\s*\{{(.*?)\}};"
    match = re.search(pattern, text, re.S)
    if not match:
        raise SystemExit(f"array {array_name!r} not found in {header}")
    values = re.findall(r"0x[0-9a-fA-F]+", match.group(1))
    if not values:
        raise SystemExit(f"array {array_name!r} is empty in {header}")
    return bytes(int(v, 16) for v in values)


def extract_flash_region(dump: Path, offset: int, size: int) -> bytes:
    data = dump.read_bytes()
    end = offset + size
    if end > len(data):
        raise SystemExit(
            f"flash dump too small: need {end} bytes, have {len(data)}"
        )
    return data[offset:end]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, required=True, help="output .bin path")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--header", type=Path, help="C header with byte array")
    src.add_argument("--copy", type=Path, help="existing firmware .bin to copy")
    src.add_argument("--flash-dump", type=Path, help="full SPI flash dump")
    ap.add_argument("--array", help="array name when using --header")
    ap.add_argument(
        "--offset",
        type=lambda s: int(s, 0),
        default=0x106000,
        help="byte offset for --flash-dump (default: FM region from dirb.tech)",
    )
    ap.add_argument(
        "--size",
        type=lambda s: int(s, 0),
        default=0x81721,
        help="byte length for --flash-dump (default: FM V5.1.0 size)",
    )
    args = ap.parse_args()

    if args.header is not None:
        if not args.array:
            ap.error("--array is required with --header")
        payload = extract_c_array(args.header, args.array)
    elif args.copy is not None:
        payload = args.copy.read_bytes()
    else:
        payload = extract_flash_region(args.flash_dump, args.offset, args.size)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(payload)
    print(f"wrote {args.out} ({len(payload)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
