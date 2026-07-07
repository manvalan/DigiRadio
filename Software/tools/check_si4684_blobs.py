#!/usr/bin/env python3
"""check_si4684_blobs.py — CI gate: no proprietary Si4684 blobs in git.

DigiRadio firmware — https://github.com/manvalan/DigiRadio

Copyright 2026 Michele Bigi
SPDX-License-Identifier: Apache-2.0

Skyworks / uGreen Si4684 application images must live only on the builder's
machine under Firmware/Si4684-Firmware/*.bin (gitignored). This script fails
if any such blob is tracked, appears in git history, or is missing from
.gitignore.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FW_DIR = ROOT / "Firmware" / "Si4684-Firmware"
GITIGNORE = ROOT / ".gitignore"
IGNORE_LINE = "Firmware/Si4684-Firmware/*.bin"


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=ROOT.parent,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def check_gitignore() -> list[str]:
    errors: list[str] = []
    if not GITIGNORE.is_file():
        errors.append(f"missing {GITIGNORE.relative_to(ROOT)}")
        return errors
    text = GITIGNORE.read_text(encoding="utf-8")
    if IGNORE_LINE not in text:
        errors.append(
            f".gitignore must contain {IGNORE_LINE!r} "
            "(proprietary Si4684 blobs)"
        )
    return errors


def check_tracked_bins() -> list[str]:
    out = git("ls-files", "--", "Software/Firmware/Si4684-Firmware/*.bin")
    tracked = [line for line in out.splitlines() if line.strip()]
    if not tracked:
        return []
    return [
        "tracked Si4684 blob(s) in git index (remove and keep local only): "
        + ", ".join(tracked)
    ]


def check_history() -> list[str]:
    out = git(
        "log",
        "--all",
        "--oneline",
        "--",
        "Software/Firmware/Si4684-Firmware/*.bin",
    )
    lines = [line for line in out.splitlines() if line.strip()]
    if not lines:
        return []
    return [
        "Si4684 .bin file(s) found in git history — "
        "history rewrite required before publishing: "
        + "; ".join(lines[:5])
        + (" …" if len(lines) > 5 else "")
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Ensure Si4684 firmware blobs are not in git."
    )
    parser.parse_args()
    errors = check_gitignore() + check_tracked_bins() + check_history()
    if errors:
        for err in errors:
            print(f"check_si4684_blobs: error: {err}", file=sys.stderr)
        return 1
    print(
        "Si4684 blob policy OK: "
        f"{IGNORE_LINE} gitignored, no tracked blobs, clean history."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
