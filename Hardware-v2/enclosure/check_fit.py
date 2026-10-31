#!/usr/bin/env python3
"""Check the built shells against the board STEP and the battery, in both orientations.

Usage: check_fit.py   (after digiradio_v2_case.py)
"""
from pathlib import Path

import cadquery as cq

HERE = Path(__file__).parent
BOARD = HERE.parent / "3d" / "digiradio-v2.step"
BATTERIES = {
    "60 mm along x": (25.0, 85.0, -171.4, -121.4),
    "60 mm along y": (30.0, 80.0, -171.4, -111.4),
}
BAT_Z0, BAT_T = -9.75, 7.5


def boxes_touch(a, b):
    p, q = a.BoundingBox(), b.BoundingBox()
    return p.xmin < q.xmax and q.xmin < p.xmax and p.ymin < q.ymax and q.ymin < p.ymax and p.zmin < q.zmax and q.zmin < p.zmax


def clash(a, solids):
    return sum(a.intersect(s).Volume() for s in solids if boxes_touch(a, s))


def main():
    board = cq.importers.importStep(str(BOARD)).val().Solids()
    shells = {n: cq.importers.importStep(str(HERE / "step" / f"{n}.step")).val() for n in ("lower_body", "upper_body", "upper_light_pipe")}
    for name, shell in shells.items():
        print(f"{name}: board clash {clash(shell, board):.4f} mm3, valid {shell.isValid()}")
    print(f"upper/lower clash {shells['upper_body'].intersect(shells['lower_body']).Volume():.4f} mm3")
    for name, (x0, x1, y0, y1) in BATTERIES.items():
        cell = cq.Workplane("XY").box(x1 - x0, y1 - y0, BAT_T, centered=False).translate((x0, y0, BAT_Z0)).val()
        print(f"battery {name}: shell clash {clash(cell, [shells['lower_body']]):.4f}, board clash {clash(cell, board):.4f} mm3")


if __name__ == "__main__":
    main()
