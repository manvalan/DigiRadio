#!/usr/bin/env python3
"""Turn the "EEP" lines of a serial log into dsp/adau1452_selfboot.bin.

Usage: tools/eeprom_log_to_bin.py monitor.log -o dsp/adau1452_selfboot.bin
Stops on a gap in the addresses and drops the blank 0xFF tail. Check that the
first byte is the 0xAA sentinel of the data sheet's self boot header.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

LINE = re.compile(r"EEP ([0-9A-F]{5}):((?: [0-9A-F]{2})+)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("log", type=Path)
    parser.add_argument("-o", "--out", type=Path, required=True)
    args = parser.parse_args()

    image = bytearray()
    text = args.log.read_text(errors="replace")
    for address, data in LINE.findall(text):
        if int(address, 16) != len(image):
            print(f"gap at 0x{address}: log is incomplete", file=sys.stderr)
            return 1
        image += bytes.fromhex(data)
    image = image.rstrip(b"\xff")
    if not image or image[0] != 0xAA:
        print("no self boot sentinel 0xAA at address 0", file=sys.stderr)
        return 1
    args.out.write_bytes(image)
    print(f"{args.out}: {len(image)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
