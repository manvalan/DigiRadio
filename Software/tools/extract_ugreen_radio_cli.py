#!/usr/bin/env python3
"""extract_ugreen_radio_cli.py — pull Si468x blobs from uGreen radio_cli ELF.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

uGreen embeds Skyworks firmware inside ``radio_cli`` (Files_v16.zip). This module
locates ``fw_*`` rodata symbols and writes raw ``.bin`` payloads for DigiRadio
HOST_LOAD boot (AN649 / PE5PVB flow).

Obtain Files_v16.zip from https://ugreen.eu/downloads/ (DABBoard customer pack).
Do not redistribute extracted blobs in public repositories.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path

# Exact HOST_LOAD sizes observed in radio_cli power_up_and_load (ARM64 builds).
UGREEN_KNOWN_SIZES: dict[str, int] = {
    "fw_rom00_patch016bin": 5796,
    "fw_dab_radio_5_0_5bin": 521_448,
    "fw_dab_radio_6_0_6bin": 496_944,
    "fw_fmhd_radio_5_0_4bin": 530_184,
    "fw_fmhd_radio_5_1_3bin": 531_300,
}

FM_SYMBOL_PRIORITY = (
    "fw_fmhd_radio_5_1_3bin",
    "fw_fmhd_radio_5_1_0bin",
    "fw_fm_radio_5_1_0bin",
    "fw_fmhd_radio_5_0_4bin",
    "fw_fm_radio_5_0_9bin",
)

DAB_SYMBOL_PRIORITY = (
    "fw_dab_radio_6_0_6bin",
    "fw_dab_radio_5_0_8bin",
    "fw_dab_radio_5_0_5bin",
    "fw_dab_radio_4_0_5bif",
)

PATCH_SYMBOL = "fw_rom00_patch016bin"

RADIO_CLI_GLOB = ("radio_cli_v*", "radio_cli")


@dataclass(frozen=True)
class FirmwareBlob:
    symbol: str
    offset: int
    size: int
    data: bytes


def resolve_radio_cli(path: Path) -> Path:
    """Return path to radio_cli ELF inside a file, zip, or directory."""
    path = path.expanduser().resolve()
    if path.is_file() and zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as zf:
            names = [
                n
                for n in zf.namelist()
                if "radio_cli" in Path(n).name and not n.endswith("/")
            ]
            if not names:
                raise SystemExit(f"No radio_cli binary found inside {path}")
            # Prefer 64-bit builds (same blobs, easier to test on desktop nm).
            names.sort(key=lambda n: ("64-bit" not in n, n))
            chosen = names[0]
            tmp_dir = Path(path.parent, f".ugreen-extract-{path.stem}")
            tmp_dir.mkdir(exist_ok=True)
            out = tmp_dir / Path(chosen).name
            if not out.exists() or out.stat().st_mtime < path.stat().st_mtime:
                out.write_bytes(zf.read(chosen))
            return out

    if path.is_dir():
        for pattern in ("**/bin/64-bit/radio_cli*", "**/bin/32-bit/radio_cli*"):
            matches = sorted(path.glob(pattern))
            if matches:
                return matches[0]
        for pattern in RADIO_CLI_GLOB:
            matches = sorted(path.rglob(pattern))
            if matches:
                return matches[0]
        raise SystemExit(f"No radio_cli binary under {path}")

    if path.is_file():
        return path

    raise SystemExit(f"Path not found: {path}")


def _run_nm(elf: Path) -> str:
    for cmd in (["llvm-nm", str(elf)], ["nm", str(elf)]):
        try:
            proc = subprocess.run(
                cmd, capture_output=True, text=True, check=False
            )
        except FileNotFoundError:
            continue
        if proc.returncode == 0 and proc.stdout.strip():
            return proc.stdout
    raise SystemExit(
        f"Could not read symbols from {elf} (install nm or llvm-nm)"
    )


def _parse_nm_output(text: str) -> dict[str, int]:
    symbols: dict[str, int] = {}
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 3:
            continue
        addr_s, sym_type, name = parts[0], parts[1], parts[2]
        if sym_type not in {"R", "D", "r"}:
            continue
        if not name.startswith("fw_"):
            continue
        try:
            symbols[name] = int(addr_s, 16)
        except ValueError:
            continue
    return symbols


def _elf_vma_to_file_offset(elf: Path, vma: int) -> int:
    data = elf.read_bytes()
    if data[:4] != b"\x7fELF":
        raise SystemExit(f"Not an ELF file: {elf}")

    ei_class = data[4]
    if ei_class == 1:
        e_shoff = struct.unpack_from("<I", data, 0x20)[0]
        e_shentsize = struct.unpack_from("<H", data, 0x2E)[0]
        e_shnum = struct.unpack_from("<H", data, 0x30)[0]
        e_shstrndx = struct.unpack_from("<H", data, 0x32)[0]
        sh_fmt = "<IIIIIIII"
    elif ei_class == 2:
        e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
        e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
        e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
        e_shstrndx = struct.unpack_from("<H", data, 0x3E)[0]
        sh_fmt = "<IIQQQQIIQQ"
    else:
        raise SystemExit(f"Unsupported ELF class in {elf}")

    if e_shnum == 0:
        return vma

    def read_section(index: int) -> tuple[int, int, int]:
        off = e_shoff + index * e_shentsize
        fields = struct.unpack_from(sh_fmt, data, off)
        if ei_class == 1:
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = fields[:6]
        else:
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = fields[:6]
        return sh_addr, sh_offset, sh_size

    shstr_off = read_section(e_shstrndx)[1]

    for index in range(e_shnum):
        sh_addr, sh_offset, sh_size = read_section(index)
        if sh_size == 0:
            continue
        if sh_addr <= vma < sh_addr + sh_size:
            return sh_offset + (vma - sh_addr)

    return vma


def _pick_symbol(symbols: dict[str, int], priority: tuple[str, ...]) -> str | None:
    for name in priority:
        if name in symbols:
            return name
    return None


def _blob_size(
    symbol: str,
    vma: int,
    ordered: list[tuple[str, int]],
    file_size: int,
    file_offset: int,
) -> int:
    if symbol in UGREEN_KNOWN_SIZES:
        return UGREEN_KNOWN_SIZES[symbol]

    for name, addr in ordered:
        if addr > vma:
            return addr - vma

    return file_size - file_offset


def extract_blob(elf: Path, symbol: str, symbols: dict[str, int]) -> FirmwareBlob:
    if symbol not in symbols:
        raise SystemExit(f"Symbol {symbol!r} not found in {elf}")

    vma = symbols[symbol]
    file_offset = _elf_vma_to_file_offset(elf, vma)
    ordered = sorted(symbols.items(), key=lambda item: item[1])
    size = _blob_size(
        symbol, vma, ordered, elf.stat().st_size, file_offset
    )

    data = elf.read_bytes()
    end = file_offset + size
    if end > len(data):
        raise SystemExit(
            f"{symbol}: need {size} bytes at file offset 0x{file_offset:x}, "
            f"ELF size is {len(data)}"
        )

    payload = data[file_offset:end]
    if len(payload) != size:
        raise SystemExit(f"{symbol}: short read ({len(payload)} != {size})")

    return FirmwareBlob(symbol, file_offset, size, payload)


def extract_images(
    source: Path,
    *,
    want_fm: bool = True,
    want_dab: bool = False,
    want_patch: bool = False,
) -> dict[str, FirmwareBlob]:
    elf = resolve_radio_cli(source)
    symbols = _parse_nm_output(_run_nm(elf))
    if not symbols:
        raise SystemExit(f"No fw_* symbols in {elf}")

    out: dict[str, FirmwareBlob] = {}

    if want_patch:
        out["patch"] = extract_blob(elf, PATCH_SYMBOL, symbols)

    if want_dab:
        dab_sym = _pick_symbol(symbols, DAB_SYMBOL_PRIORITY)
        if dab_sym is None:
            raise SystemExit(f"No DAB fw_* symbol in {elf}")
        out["dab"] = extract_blob(elf, dab_sym, symbols)

    if want_fm:
        fm_sym = _pick_symbol(symbols, FM_SYMBOL_PRIORITY)
        if fm_sym is None:
            raise SystemExit(f"No FM fw_* symbol in {elf}")
        out["fm"] = extract_blob(elf, fm_sym, symbols)

    return out


def write_blobs(blobs: dict[str, FirmwareBlob], out_dir: Path) -> None:
    mapping = {
        "patch": "rom_patch_016.bin",
        "dab": "dab_firmware.bin",
        "fm": "fm_firmware.bin",
    }
    out_dir.mkdir(parents=True, exist_ok=True)
    for key, blob in blobs.items():
        dest = out_dir / mapping[key]
        dest.write_bytes(blob.data)
        print(
            f"wrote {dest} ({len(blob.data)} bytes) "
            f"from {blob.symbol} @ 0x{blob.offset:x}"
        )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "source",
        type=Path,
        help="radio_cli ELF, Files_v16.zip, or unpacked Files_v16 directory",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "Firmware" / "Si4684-Firmware",
        help="output directory for .bin files",
    )
    ap.add_argument("--fm", action="store_true", help="extract FM image")
    ap.add_argument("--dab", action="store_true", help="extract DAB image")
    ap.add_argument("--patch", action="store_true", help="extract ROM patch")
    ap.add_argument(
        "--all",
        action="store_true",
        help="extract patch, DAB, and FM",
    )
    args = ap.parse_args()

    if args.all:
        want_fm = want_dab = want_patch = True
    else:
        want_fm = args.fm
        want_dab = args.dab
        want_patch = args.patch
        if not (want_fm or want_dab or want_patch):
            want_fm = True

    blobs = extract_images(
        args.source,
        want_fm=want_fm,
        want_dab=want_dab,
        want_patch=want_patch,
    )
    write_blobs(blobs, args.out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
