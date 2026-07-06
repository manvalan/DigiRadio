#!/usr/bin/env python3
"""check-manual-sync.py — enforce AGENTS.md §3.4.

Every public, architecturally-significant class (declared in an
`include/` directory) must have a matching manual section tagged
`\\label{cls:ClassName}` in the LaTeX manual sources.

Exit code 0 if every public class is documented in the manual, 1
otherwise (printing the missing ones). Intended for CI, next to the
Doxygen check.

Usage:
    tools/check-manual-sync.py [--src components] [--manual docs/manual]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# A public class declaration: `class Name {` / `class Name :` / `class Name`
# but NOT a forward declaration `class Name;` and NOT `enum class`.
CLASS_RE = re.compile(
    r"^\s*(?:template\s*<[^>]*>\s*)?class\s+([A-Z]\w*)\s*(?:final\s*)?(?:[:{]|$)"
)
# A pure forward declaration: `class Name;` (optionally templated).
FORWARD_RE = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?class\s+\w+\s*;\s*$")
LABEL_RE = re.compile(r"\\label\{cls:(\w+)\}")


def public_classes(src_root: Path) -> dict[str, Path]:
    """Collect class names declared under any include/ directory."""
    found: dict[str, Path] = {}
    for header in src_root.rglob("*.hpp"):
        if "include" not in header.parts:
            continue
        for line in header.read_text(encoding="utf-8").splitlines():
            if FORWARD_RE.match(line):           # forward declaration
                continue
            if "enum class" in line:
                continue
            m = CLASS_RE.match(line)
            if m:
                found.setdefault(m.group(1), header)
    return found


def documented_classes(manual_root: Path) -> set[str]:
    """Collect class names that have a \\label{cls:...} in the manual."""
    labelled: set[str] = set()
    for tex in manual_root.rglob("*.tex"):
        labelled.update(LABEL_RE.findall(tex.read_text(encoding="utf-8")))
    return labelled


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default="components",
                    help="root of the source tree (default: components)")
    ap.add_argument("--manual", default="docs/manual",
                    help="root of the manual sources (default: docs/manual)")
    args = ap.parse_args()

    src_root = Path(args.src)
    manual_root = Path(args.manual)
    if not src_root.exists():
        print(f"source root not found: {src_root}", file=sys.stderr)
        return 2
    if not manual_root.exists():
        print(f"manual root not found: {manual_root}", file=sys.stderr)
        return 2

    classes = public_classes(src_root)
    documented = documented_classes(manual_root)

    missing = {name: path for name, path in classes.items()
               if name not in documented}

    if missing:
        print("Manual out of sync — missing \\label{cls:...} sections:")
        for name, path in sorted(missing.items()):
            print(f"  - {name}  (declared in {path})")
        print(f"\n{len(missing)} public class(es) undocumented in the manual.")
        return 1

    print(f"Manual in sync: {len(classes)} public class(es) documented.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
