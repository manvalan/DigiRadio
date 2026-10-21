#!/usr/bin/env python3
"""igiRadio AppIcon 1024×1024: stylized portable radio, full-bleed."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "igiRadio/Resources/Assets.xcassets/AppIcon.appiconset"

C_LIGHT = (115, 163, 255)
C_DEEP = (59, 115, 250)
C_INK = (36, 82, 210)
C_FACE = (232, 240, 255)
WHITE = (255, 255, 255)


def lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def lerp_color(
    c1: tuple[int, int, int], c2: tuple[int, int, int], t: float
) -> tuple[int, int, int]:
    t = max(0.0, min(1.0, t))
    return (
        int(lerp(c1[0], c2[0], t)),
        int(lerp(c1[1], c2[1], t)),
        int(lerp(c1[2], c2[2], t)),
    )


def paint_background(size: int) -> Image.Image:
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (x / size) * 0.45 + (y / size) * 0.55
            px[x, y] = lerp_color(C_LIGHT, C_DEEP, t)
    return img


def box(x0: float, y0: float, x1: float, y1: float) -> tuple[int, int, int, int]:
    return (int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1)))


def draw_icon(size: int = 1024) -> Image.Image:
    s = size / 1024.0
    img = paint_background(size)
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(layer, "RGBA")

    body = box(148 * s, 312 * s, 876 * s, 860 * s)
    handle_w = max(10, int(36 * s))

    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    sdraw = ImageDraw.Draw(shadow)
    sdraw.rounded_rectangle(
        box(body[0], body[1] + 20 * s, body[2], body[3] + 24 * s),
        radius=int(96 * s),
        fill=(20, 40, 120, 60),
    )
    layer = Image.alpha_composite(
        layer, shadow.filter(ImageFilter.GaussianBlur(int(18 * s)))
    )
    draw = ImageDraw.Draw(layer, "RGBA")

    # Handle first; cabinet covers the bottom of the hoop.
    draw.rounded_rectangle(
        box(286 * s, 158 * s, 738 * s, 400 * s),
        radius=int(72 * s),
        outline=WHITE,
        width=handle_w,
    )

    # Antenna + broadcast arcs
    ax0, ay0 = 800 * s, 312 * s
    ax1, ay1 = 900 * s, 118 * s
    draw.line((ax0, ay0, ax1, ay1), fill=WHITE, width=max(6, int(15 * s)))
    tip = 13 * s
    draw.ellipse(box(ax1 - tip, ay1 - tip, ax1 + tip, ay1 + tip), fill=WHITE)
    for r, w in ((52, 11), (86, 10), (120, 9)):
        draw.arc(
            box(ax1 - r * s, ay1 - r * s, ax1 + r * s, ay1 + r * s),
            start=208,
            end=332,
            fill=WHITE,
            width=max(4, int(w * s)),
        )

    # Solid cabinet
    draw.rounded_rectangle(body, radius=int(96 * s), fill=WHITE)
    draw.rounded_rectangle(
        box(186 * s, 350 * s, 838 * s, 822 * s),
        radius=int(74 * s),
        fill=C_FACE,
    )

    # Speaker grille
    draw.rounded_rectangle(
        box(216 * s, 380 * s, 508 * s, 728 * s),
        radius=int(46 * s),
        fill=C_INK,
    )
    slat_count = 6
    inner_x0, inner_y0, inner_x1, inner_y1 = 242 * s, 408 * s, 482 * s, 700 * s
    gap = (inner_x1 - inner_x0) / slat_count
    slat_w = gap * 0.40
    for i in range(slat_count):
        x = inner_x0 + i * gap + (gap - slat_w) / 2
        draw.rounded_rectangle(
            box(x, inner_y0, x + slat_w, inner_y1),
            radius=int(9 * s),
            fill=WHITE,
        )

    # Tuning dial
    cx, cy = 694 * s, 518 * s
    outer_r = 122 * s
    draw.ellipse(
        box(cx - outer_r, cy - outer_r, cx + outer_r, cy + outer_r),
        fill=C_INK,
    )
    ring_r = 104 * s
    draw.ellipse(
        box(cx - ring_r, cy - ring_r, cx + ring_r, cy + ring_r),
        outline=WHITE,
        width=max(5, int(11 * s)),
    )
    for deg in range(0, 360, 30):
        rad = math.radians(deg - 90)
        inner = outer_r - 36 * s
        outer = outer_r - 16 * s
        draw.line(
            (
                cx + inner * math.cos(rad),
                cy + inner * math.sin(rad),
                cx + outer * math.cos(rad),
                cy + outer * math.sin(rad),
            ),
            fill=WHITE,
            width=max(3, int(7 * s)),
        )
    needle = math.radians(-40 - 90)
    nx = cx + (outer_r - 30 * s) * math.cos(needle)
    ny = cy + (outer_r - 30 * s) * math.sin(needle)
    draw.line((cx, cy, nx, ny), fill=WHITE, width=max(5, int(12 * s)))
    hub = 17 * s
    draw.ellipse(box(cx - hub, cy - hub, cx + hub, cy + hub), fill=WHITE)

    # Frequency window
    draw.rounded_rectangle(
        box(576 * s, 664 * s, 812 * s, 722 * s),
        radius=int(16 * s),
        fill=C_INK,
    )
    for i, h in enumerate((16, 9, 24, 11, 18, 8, 14)):
        x = 598 * s + i * 28 * s
        y1 = 710 * s
        draw.rectangle(box(x, y1 - h * s, x + 6 * s, y1), fill=WHITE)

    # Knobs
    for kx in (276 * s, 378 * s):
        ky = 776 * s
        kr = 28 * s
        draw.ellipse(box(kx - kr, ky - kr, kx + kr, ky + kr), fill=C_INK)
        ir = 10 * s
        draw.ellipse(
            box(kx - ir, ky - ir - 5 * s, kx + ir, ky + ir - 5 * s), fill=WHITE
        )

    img = Image.alpha_composite(img.convert("RGBA"), layer)
    return img.convert("RGB")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    img = draw_icon(1024)
    path = OUT / "AppIcon-1024.png"
    img.save(path, "PNG", optimize=True)
    print(f"Wrote {path}")
    mark_dir = ROOT / "igiRadio/Resources/Assets.xcassets/RadioMark.imageset"
    mark_dir.mkdir(parents=True, exist_ok=True)
    mark = mark_dir / "RadioMark.png"
    img.save(mark, "PNG", optimize=True)
    print(f"Wrote {mark}")


if __name__ == "__main__":
    main()
