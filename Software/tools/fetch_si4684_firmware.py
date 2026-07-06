#!/usr/bin/env python3
"""fetch_si4684_firmware.py — populate Firmware/Si4684-Firmware/ blobs.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

DAB images are extracted from PE5PVB/SI4684-DAB-Receiver (same as Slice 3).
FM images are proprietary Skyworks blobs — supply via --si46xx-dir (eval CD /
dabpi si46xx_firmware folder), --flash-dump (TechniSat-style SPI readout), or
--from-ugreen-radio-cli (uGreen DABBoard Files_v16.zip / radio_cli ELF).

AN649 Si4684-A10: FM application image fm_radio_5_1_0.bin (or community
fmhd_radio_5_0_4.bin on Si4688-class boards — verify on hardware).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path

_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from extract_ugreen_radio_cli import extract_images, write_blobs  # noqa: E402

ROOT = _TOOLS.parent
FW_DIR = ROOT / "Firmware" / "Si4684-Firmware"
EXTRACT = _TOOLS / "extract_si4684_blob.py"
PE5PVB = "https://github.com/PE5PVB/SI4684-DAB-Receiver.git"

FM_CANDIDATE_NAMES = (
    "fm_radio_5_1_0.bin",
    "fm_radio_5_0_9.bin",
    "fmhd_radio_5_1_0.bin",
    "fmhd_radio_5_0_4.bin",
)


def run_extract(args: list[str]) -> None:
    cmd = [sys.executable, str(EXTRACT), *args]
    subprocess.run(cmd, check=True)


def clone_pe5pvb(tmp: Path) -> Path:
    dest = tmp / "SI4684-DAB-Receiver"
    if not dest.exists():
        subprocess.run(
            ["git", "clone", "--depth", "1", PE5PVB, str(dest)],
            check=True,
        )
    return dest


def refresh_dab(tmp: Path) -> None:
    repo = clone_pe5pvb(tmp)
    run_extract(
        [
            "--header",
            str(repo / "src" / "Si468xROM.h"),
            "--array",
            "rom_patch_016",
            "--out",
            str(FW_DIR / "rom_patch_016.bin"),
        ]
    )
    run_extract(
        [
            "--header",
            str(repo / "src" / "firmware.h"),
            "--array",
            "firmware",
            "--out",
            str(FW_DIR / "dab_firmware.bin"),
        ]
    )


def find_fm_source(si46xx_dir: Path) -> Path | None:
    for name in FM_CANDIDATE_NAMES:
        candidate = si46xx_dir / name
        if candidate.is_file():
            return candidate
    return None


def import_fm(source: Path) -> None:
    run_extract(["--copy", str(source), "--out", str(FW_DIR / "fm_firmware.bin")])


def import_fm_from_flash(dump: Path, offset: int, size: int) -> None:
    run_extract(
        [
            "--flash-dump",
            str(dump),
            "--offset",
            hex(offset),
            "--size",
            hex(size),
            "--out",
            str(FW_DIR / "fm_firmware.bin"),
        ]
    )


def import_from_ugreen(
    source: Path,
    *,
    want_fm: bool,
    want_dab: bool,
    want_patch: bool,
) -> None:
    blobs = extract_images(
        source,
        want_fm=want_fm,
        want_dab=want_dab,
        want_patch=want_patch,
    )
    write_blobs(blobs, FW_DIR)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--si46xx-dir",
        type=Path,
        help="folder with Skyworks .bin files (eval CD / dabpi si46xx_firmware)",
    )
    ap.add_argument(
        "--flash-dump",
        type=Path,
        help="full external SPI dump (TechniSat-style layout)",
    )
    ap.add_argument(
        "--fm-offset",
        type=lambda s: int(s, 0),
        default=0x106000,
        help="FM image offset inside --flash-dump",
    )
    ap.add_argument(
        "--fm-size",
        type=lambda s: int(s, 0),
        default=0x81721,
        help="FM image size inside --flash-dump",
    )
    ap.add_argument(
        "--dab-only",
        action="store_true",
        help="refresh DAB + patch only (skip FM)",
    )
    ap.add_argument(
        "--from-ugreen-radio-cli",
        type=Path,
        metavar="PATH",
        help="uGreen Files_v16.zip, radio_cli ELF, or unpacked Files_v16 folder",
    )
    ap.add_argument(
        "--ugreen-all",
        action="store_true",
        help="with --from-ugreen-radio-cli: extract patch, DAB, and FM from radio_cli",
    )
    args = ap.parse_args()

    FW_DIR.mkdir(parents=True, exist_ok=True)

    ugreen = args.from_ugreen_radio_cli is not None
    if ugreen and args.dab_only:
        ap.error("--dab-only cannot be combined with --from-ugreen-radio-cli")

    fm_sources = int(args.si46xx_dir is not None) + int(
        args.flash_dump is not None
    ) + int(ugreen)
    if fm_sources > 1:
        ap.error(
            "choose one FM source: --si46xx-dir, --flash-dump, or "
            "--from-ugreen-radio-cli"
        )

    with tempfile.TemporaryDirectory(prefix="digiradio-si4684-") as tmp_name:
        tmp = Path(tmp_name)

        if args.dab_only:
            refresh_dab(tmp)
            print("DAB + patch refreshed; FM unchanged.")
            return 0

        ugreen_full = ugreen and args.ugreen_all
        if not ugreen_full:
            refresh_dab(tmp)

        if ugreen:
            import_from_ugreen(
                args.from_ugreen_radio_cli,
                want_fm=True,
                want_dab=ugreen_full,
                want_patch=ugreen_full,
            )
        elif args.si46xx_dir is not None:
            fm = find_fm_source(args.si46xx_dir)
            if fm is None:
                print(
                    f"No FM image found in {args.si46xx_dir}; expected one of:",
                    ", ".join(FM_CANDIDATE_NAMES),
                    file=sys.stderr,
                )
                return 1
            import_fm(fm)
        elif args.flash_dump is not None:
            import_fm_from_flash(args.flash_dump, args.fm_offset, args.fm_size)
        else:
            print(
                "FM image not updated: pass --si46xx-dir, --flash-dump, or "
                "--from-ugreen-radio-cli.\n"
                "Skyworks fm_radio_5_1_0.bin is on the Si4684 eval CD "
                "(see AN649 table 1).",
                file=sys.stderr,
            )
            return 1

    fm_path = FW_DIR / "fm_firmware.bin"
    if fm_path.is_file():
        print(f"FM image ready: {fm_path} ({fm_path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
