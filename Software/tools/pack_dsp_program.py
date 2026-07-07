#!/usr/bin/env python3
"""pack_dsp_program.py — build a DRAD v1 ADAU1701 program blob for POST /api/dsp/program.

Reads a JSON file describing SigmaStudio register writes:

[
  {"address": 2076, "data": [0, 28]},
  {"address": 1024, "data": [0, 0, ...]}
]

Usage:
  python3 tools/pack_dsp_program.py writes.json -o dsp_program.bin
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import zlib
from pathlib import Path
from typing import Any

MAGIC = b"DRAD"
VERSION = 1


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def pack_writes(writes: list[dict[str, Any]]) -> bytes:
    payload = bytearray()
    for entry in writes:
        address = int(entry["address"])
        data = bytes(int(x) & 0xFF for x in entry["data"])
        if not data:
            raise ValueError(f"empty data at address 0x{address:04X}")
        payload += struct.pack("<HH", address, len(data))
        payload += data

    header = bytearray()
    header += MAGIC
    header += struct.pack("<HH", VERSION, len(writes))
    header += struct.pack("<I", crc32(payload))
    return bytes(header + payload)


def main() -> int:
    parser = argparse.ArgumentParser(description="Pack ADAU1701 DSP program blob")
    parser.add_argument("json_file", type=Path, help="JSON array of register writes")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .bin")
    args = parser.parse_args()

    writes = json.loads(args.json_file.read_text(encoding="utf-8"))
    if not isinstance(writes, list) or not writes:
        print("JSON must be a non-empty array", file=sys.stderr)
        return 1

    blob = pack_writes(writes)
    args.output.write_bytes(blob)
    print(f"Wrote {len(blob)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
