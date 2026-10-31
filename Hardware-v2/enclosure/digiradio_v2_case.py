#!/usr/bin/env python3
"""igiRadio v2 two-part snap-fit enclosure, "Hi-Fi" style, for a Bambu Lab X1C (CadQuery).

All coordinates are the ones of Hardware-v2/3d/digiradio-v2.step, in mm:
the board spans x 25..85, y -180..-100, z 0..1.6, with the connectors on the
y = -100 edge and the SMA on the x = 25 edge.

Usage: digiradio_v2_case.py [output_dir]
"""
import json
import sys
import zipfile
from pathlib import Path

import cadquery as cq

# Board
BX0, BX1, BY0, BY1, BT = 25.0, 85.0, -180.0, -100.0, 1.6

# Shell
WALL = 2.0
GAP_X = 0.8  # side gap: the pocket takes the 60 mm battery side across x
GAP_Y = 0.3  # TOSLINK sticks out 2.31 mm, so 0.3 + WALL leaves it flush
ROOF_T = 1.6
FLOOR_T = 1.6
IX0, IX1, IY0, IY1 = BX0 - GAP_X, BX1 + GAP_X, BY0 - GAP_Y, BY1 + GAP_Y
OX0, OX1, OY0, OY1 = IX0 - WALL, IX1 + WALL, IY0 - WALL, IY1 + WALL
CORNER_R = 2.0

# Heights
ZS = 2.07  # split plane, on the SMA axis
Z_ROOF_IN = 12.0  # TOSLINK top is at 11.6
Z_TOP = Z_ROOF_IN + ROOF_T
BAT_T = 7.5
BAT_SWELL = 0.9
Z_BAT_TOP = -1.4  # lowest part over the pocket is the SMA body, at -1.18
Z_FLOOR_IN = Z_BAT_TOP - BAT_T - BAT_SWELL
Z_BOTTOM = Z_FLOOR_IN - FLOOR_T
INLAY = 0.4

# Connectors
USB = dict(x0=41.49, x1=50.43, z0=1.59, z1=4.80, face_y=-98.41)
TOS = dict(x0=28.19, x1=37.89, z0=1.59, z1=11.60)
SMA = dict(y=-119.39, z=2.07, neck_r=2.65, flange=6.5, flange_x=23.4)
USB_CL, TOS_CL, SMA_CL = 0.25, 0.15, 0.15
# Room for a USB-C overmould, which would otherwise hit the wall
USB_POCKET_W, USB_POCKET_H = 13.0, 7.2

# Battery pocket, stops at both ends; either battery side can go along y
POCKET_Y0, POCKET_Y1 = -171.5, -110.0
STOP_T, Z_STOP_TOP = 1.2, -6.5

# Board supports
LEDGE_W = 1.8  # from the wall, so 1.0 mm under the board
LOCATOR_CL = 0.15
LEFT_LEDGE_KEEPOUT = (-123.0, -115.7)  # SMA legs
FAR_LEDGES = [(25.0, 40.0), (52.0, 85.0)]  # USB shell legs go through the board
LOCATOR_YS_LEFT = [-172.0, -152.0, -127.0]
LOCATOR_YS_RIGHT = [-160.0, -128.0, -114.0]

# Top-side clamp ribs, placed on edge spans free of parts in the STEP
RIB_W, RIB_GAP = 1.6, 0.05
RIBS_LEFT = [(-165.5, -161.2), (-145.0, -135.0), (-113.0, -103.0)]
RIBS_RIGHT = [(-176.0, -166.0), (-150.0, -140.0), (-109.5, -102.0)]
RIBS_NEAR = [(30.0, 40.0), (64.0, 74.0)]
RIBS_FAR = [(53.0, 62.0)]

# Lip
LIP_T, LIP_H, LIP_CL = 0.9, 1.6, 0.1

# Snap hooks: the arm sits in an outer recess and hooks through the wall
SNAP_YS_LEFT = [-168.0, -135.0, -106.0]
SNAP_YS_RIGHT = [-168.0, -135.0, -106.0]
ARM_T, ARM_W, ARM_END = 1.0, 7.0, -5.6
RECESS_D, RECESS_W, RECESS_END = 1.1, 7.6, -7.0
HOOK_D, HOOK_TOP, HOOK_FLAT, HOOK_RAMP = 0.6, -3.35, 1.05, 1.0
WINDOW_W, WINDOW_TOP, WINDOW_BOTTOM = 6.4, -3.2, -5.6
RELIEF_W = 0.5

# Reset button: printed cantilever with a rod down to the switch.
# BOOT stays inside: USB flashing does not need it in normal use.
BUTTONS = [("RESET", 74.65, -103.07)]
SWITCH_TOP = 4.09
ROD_D, ROD_GAP = 2.6, 0.25
TAB_W, TAB_SLOT, TAB_T, TAB_TIP_Y, TAB_ROOT_Y, TAB_THIN_TO_Y = 5.0, 0.6, 1.0, -100.4, -114.0, -112.0
CAP_D = 3.6

# Status LED D3 light pipe, inside an opaque sleeve
LED = dict(x=27.78, y=-167.69, top=2.70)
PIPE_D, SLEEVE_D, PIPE_GAP, SLEEVE_GAP = 2.8, 5.0, 0.25, 0.5

# Equaliser grille over the charger, and a line across the roof
EQ_BARS = [6, 10, 15, 20, 17, 12, 8, 13, 18, 22, 16, 11, 7, 12]
EQ_X0, EQ_PITCH, EQ_W, EQ_Y0 = 35.0, 3.1, 1.7, -177.0
LINE_Y0, LINE_Y1, LINE_INSET = -152.0, -150.8, 0.8
SIDE_VENTS = [("L", -160.0), ("R", -160.0), ("R", -127.0)]
SIDE_VENT_N, SIDE_VENT_PITCH, SIDE_VENT_W = 5, 3.0, 1.4
SIDE_VENT_Z = (5.0, 10.5)

# Inlays
NAME = "igiRadio"
# AMS slots
FIL_UPPER, FIL_LOWER, FIL_ACCENT, FIL_PIPE = 1, 2, 3, 4

# Bed layout, mm
PLATE_MARGIN_X, PLATE_SPACING, PLATE_Y = 45.0, 15.0, 85.0

FONT = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
ROOF_TEXT = [
    (NAME, 55.0, -130.0, 9.0),
    ("USB-C", 45.96, -104.0, 3.5),
    ("OPT", 33.04, -104.0, 3.5),
    ("ANT", 31.0, -119.39, 3.5),
    ("RESET", 74.65, -117.0, 3.5),
]
BOTTOM_TEXT = [(NAME + " v2", 55.0, -130.0, 7.0), ("Li-Po 3.7 V", 55.0, -142.0, 4.5)]


def box(x0, x1, y0, y1, z0, z1):
    return cq.Workplane("XY").box(x1 - x0, y1 - y0, z1 - z0, centered=False).translate((x0, y0, z0))


def cyl_z(x, y, d, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(d / 2).extrude(z1 - z0)


def cyl_x(y, z, r, x0, x1):
    return cq.Workplane("YZ").workplane(offset=x0).center(y, z).circle(r).extrude(x1 - x0)


def prism_xz(points, y0, y1):
    """Extrude a polygon given in (x, z) from y0 to y1."""
    return (
        cq.Workplane("XZ").workplane(offset=-y1).polyline(points).close().extrude(y1 - y0)
    )


def union_all(items):
    out = items[0]
    for item in items[1:]:
        out = out.union(item)
    return out


def text_on(plane, entries, depth):
    parts = []
    for text, x, y, size in entries:
        parts.append(
            cq.Workplane(plane)
            .center(x, y if plane.zDir.z > 0 else -y)
            .text(text, size, -depth, fontPath=FONT, halign="center", valign="center")
        )
    return union_all(parts)


def lip_ring():
    """Inner half of the wall above the split, minus the connector zones."""
    outer = box(IX0 - LIP_T, IX1 + LIP_T, IY0 - LIP_T, IY1 + LIP_T, ZS, ZS + LIP_H)
    ring = outer.cut(box(IX0, IX1, IY0, IY1, ZS - 1, ZS + LIP_H + 1))
    ring = ring.cut(box(TOS["x0"] - 1.5, USB["x1"] + 1.5, IY1 - 1, OY1 + 1, ZS - 1, ZS + LIP_H + 1))
    ring = ring.cut(box(OX0 - 1, IX0 + 1, SMA["y"] - 5.0, SMA["y"] + 5.0, ZS - 1, ZS + LIP_H + 1))
    return ring


def connector_cuts():
    """Openings shared by both shells; each shell keeps its side of the split."""
    wall_y0, wall_y1 = IY1 - 0.5, OY1 + 1
    tos = box(TOS["x0"] - TOS_CL, TOS["x1"] + TOS_CL, wall_y0, wall_y1, TOS["z0"] - 0.2, TOS["z1"] + TOS_CL)
    usb = (
        box(USB["x0"] - USB_CL, USB["x1"] + USB_CL, wall_y0, wall_y1, USB["z0"] - USB_CL, USB["z1"] + USB_CL)
        .edges("|Y")
        .fillet(0.8)
    )
    usb_cx, usb_cz = (USB["x0"] + USB["x1"]) / 2, (USB["z0"] + USB["z1"]) / 2
    pocket = (
        box(
            usb_cx - USB_POCKET_W / 2,
            usb_cx + USB_POCKET_W / 2,
            USB["face_y"] - 0.02,
            OY1 + 1,
            usb_cz - USB_POCKET_H / 2,
            usb_cz + USB_POCKET_H / 2,
        )
        .edges("|Y")
        .fillet(2.0)
    )
    # Tight on the neck, with a square seat that stops the SMA from turning
    # while the antenna is tightened
    sma = cyl_x(SMA["y"], SMA["z"], SMA["neck_r"] + SMA_CL, OX0 - 1, IX0 + 1)
    half = SMA["flange"] / 2 + SMA_CL
    seat = box(SMA["flange_x"] - 0.1, IX0 + 1, SMA["y"] - half, SMA["y"] + half, SMA["z"] - half, SMA["z"] + half)
    return tos.union(usb).union(pocket).union(sma).union(seat)


def snap_sites():
    return [(-1, y) for y in SNAP_YS_LEFT] + [(1, y) for y in SNAP_YS_RIGHT]


def wall_span(side, from_outer, to_outer):
    """x range measured inward from the outer face of a long wall."""
    if side < 0:
        return OX0 + from_outer, OX0 + to_outer
    return OX1 - to_outer, OX1 - from_outer


def build_lower():
    body = box(OX0, OX1, OY0, OY1, Z_BOTTOM, ZS).edges("|Z").fillet(CORNER_R).faces("<Z").chamfer(0.6)
    body = body.cut(box(IX0, IX1, IY0, IY1, Z_FLOOR_IN, ZS + 1))

    ledge_z0 = Z_BAT_TOP
    for y0, y1 in [(IY0, LEFT_LEDGE_KEEPOUT[0]), (LEFT_LEDGE_KEEPOUT[1], IY1)]:
        body = body.union(
            prism_xz([(IX0, ledge_z0), (IX0 + LEDGE_W, 0), (IX0, 0)], y0, y1)
        )
    body = body.union(prism_xz([(IX1, ledge_z0), (IX1 - LEDGE_W, 0), (IX1, 0)], IY0, IY1))
    near = (
        cq.Workplane("YZ")
        .workplane(offset=IX0)
        .polyline([(IY0, ledge_z0), (IY0 + LEDGE_W, 0), (IY0, 0)])
        .close()
        .extrude(IX1 - IX0)
    )
    body = body.union(near)
    for x0, x1 in FAR_LEDGES:
        far = (
            cq.Workplane("YZ")
            .workplane(offset=x0)
            .polyline([(IY1, ledge_z0), (IY1 - LEDGE_W, 0), (IY1, 0)])
            .close()
            .extrude(x1 - x0)
        )
        body = body.union(far)

    for y in LOCATOR_YS_LEFT:
        body = body.union(box(IX0, BX0 - LOCATOR_CL, y - 2, y + 2, 0, ZS))
    for y in LOCATOR_YS_RIGHT:
        body = body.union(box(BX1 + LOCATOR_CL, IX1, y - 2, y + 2, 0, ZS))

    # Battery stops, segmented so that leads can leave anywhere
    for x0, x1 in [(IX0, 34.0), (46.0, 64.0), (76.0, IX1)]:
        body = body.union(box(x0, x1, POCKET_Y0 - STOP_T, POCKET_Y0, Z_FLOOR_IN, Z_STOP_TOP))
        body = body.union(box(x0, x1, POCKET_Y1, POCKET_Y1 + STOP_T, Z_FLOOR_IN, Z_STOP_TOP))

    body = body.union(lip_ring())
    body = body.cut(connector_cuts())

    for side, y in snap_sites():
        x0, x1 = wall_span(side, -1, RECESS_D)
        body = body.cut(box(x0, x1, y - RECESS_W / 2, y + RECESS_W / 2, RECESS_END, ZS + 1))
        x0, x1 = wall_span(side, RECESS_D - 0.1, WALL + 0.1)
        body = body.cut(box(x0, x1, y - WINDOW_W / 2, y + WINDOW_W / 2, WINDOW_BOTTOM, WINDOW_TOP))

    plane = cq.Plane(origin=(0, 0, Z_BOTTOM), xDir=(1, 0, 0), normal=(0, 0, -1))
    accent = text_on(plane, BOTTOM_TEXT, INLAY)
    body = body.cut(accent)
    return body, accent


def build_upper():
    body = box(OX0, OX1, OY0, OY1, ZS, Z_TOP).edges("|Z").fillet(CORNER_R).faces(">Z").chamfer(0.8)
    body = body.cut(box(IX0, IX1, IY0, IY1, ZS - 1, Z_ROOF_IN))

    grown = box(IX0 - LIP_T - LIP_CL, IX1 + LIP_T + LIP_CL, IY0 - LIP_T - LIP_CL, IY1 + LIP_T + LIP_CL, ZS - 1, ZS + LIP_H + LIP_CL)
    recess = grown.cut(box(IX0, IX1, IY0, IY1, ZS - 2, ZS + LIP_H + 2))
    recess = recess.cut(box(TOS["x0"] - 1.4, USB["x1"] + 1.4, IY1 - 1, OY1 + 1, ZS - 2, ZS + LIP_H + 2))
    recess = recess.cut(box(OX0 - 1, IX0 + 1, SMA["y"] - 4.9, SMA["y"] + 4.9, ZS - 2, ZS + LIP_H + 2))
    body = body.cut(recess)

    rib_z0 = BT + RIB_GAP
    for y0, y1 in RIBS_LEFT:
        body = body.union(box(IX0, BX0 + RIB_W, y0, y1, rib_z0, Z_ROOF_IN + 0.1))
    for y0, y1 in RIBS_RIGHT:
        body = body.union(box(BX1 - RIB_W, IX1, y0, y1, rib_z0, Z_ROOF_IN + 0.1))
    for x0, x1 in RIBS_NEAR:
        body = body.union(box(x0, x1, IY0, BY0 + RIB_W, rib_z0, Z_ROOF_IN + 0.1))
    for x0, x1 in RIBS_FAR:
        body = body.union(box(x0, x1, BY1 - RIB_W, IY1, rib_z0, Z_ROOF_IN + 0.1))

    # LED sleeve, tied to the wall so that the tall tube stays stiff
    z_sleeve = LED["top"] + SLEEVE_GAP
    body = body.union(cyl_z(LED["x"], LED["y"], SLEEVE_D, z_sleeve, Z_ROOF_IN + 0.1))
    body = body.union(box(IX0, LED["x"], LED["y"] - 0.8, LED["y"] + 0.8, z_sleeve, Z_ROOF_IN + 0.1))

    body = body.cut(connector_cuts())

    # Snap arms
    for side, y in snap_sites():
        x0, x1 = wall_span(side, 0, ARM_T)
        arm = box(x0, x1, y - ARM_W / 2, y + ARM_W / 2, ARM_END, ZS + 0.1)
        if side < 0:
            pts = [(OX0 + ARM_T, HOOK_TOP), (OX0 + ARM_T + HOOK_D, HOOK_TOP),
                   (OX0 + ARM_T + HOOK_D, HOOK_TOP - HOOK_FLAT), (OX0 + ARM_T, HOOK_TOP - HOOK_FLAT - HOOK_RAMP)]
        else:
            pts = [(OX1 - ARM_T, HOOK_TOP), (OX1 - ARM_T - HOOK_D, HOOK_TOP),
                   (OX1 - ARM_T - HOOK_D, HOOK_TOP - HOOK_FLAT), (OX1 - ARM_T, HOOK_TOP - HOOK_FLAT - HOOK_RAMP)]
        hook = prism_xz(pts, y - ARM_W / 2 + 0.3, y + ARM_W / 2 - 0.3)
        body = body.union(arm).union(hook)
        # Relief slits lengthen the flexing arm up to the top of the lip recess
        x0, x1 = wall_span(side, -1, WALL - LIP_T - LIP_CL + 0.02)
        for s in (-1, 1):
            yc = y + s * (ARM_W / 2 + RELIEF_W / 2)
            body = body.cut(box(x0, x1, yc - RELIEF_W / 2, yc + RELIEF_W / 2, ZS - 0.01, ZS + LIP_H))

    # Equaliser grille
    for i, height in enumerate(EQ_BARS):
        slot = (
            cq.Workplane("XY")
            .workplane(offset=Z_ROOF_IN - 1)
            .center(EQ_X0 + i * EQ_PITCH, EQ_Y0 + height / 2)
            .slot2D(height, EQ_W, 90)
            .extrude(ROOF_T + 2)
        )
        body = body.cut(slot)

    # Side vents, vertical so that they print without bridging far
    for side_name, y0 in SIDE_VENTS:
        x0, x1 = (OX0 - 1, IX0 + 1) if side_name == "L" else (IX1 - 1, OX1 + 1)
        for i in range(SIDE_VENT_N):
            yc = y0 + i * SIDE_VENT_PITCH
            body = body.cut(box(x0, x1, yc - SIDE_VENT_W / 2, yc + SIDE_VENT_W / 2, *SIDE_VENT_Z))

    # Buttons
    accents = []
    for _, x, y in BUTTONS:
        hw = TAB_W / 2
        outer = box(x - hw - TAB_SLOT, x + hw + TAB_SLOT, TAB_ROOT_Y, TAB_TIP_Y + TAB_SLOT, Z_ROOF_IN - 1, Z_TOP + 1)
        tab = box(x - hw, x + hw, TAB_ROOT_Y - 1, TAB_TIP_Y, Z_ROOF_IN - 2, Z_TOP + 2)
        body = body.cut(outer.cut(tab))
        body = body.cut(box(x - hw, x + hw, TAB_THIN_TO_Y, TAB_TIP_Y, Z_ROOF_IN - 0.1, Z_TOP - TAB_T))
        body = body.union(cyl_z(x, y, ROD_D, SWITCH_TOP + ROD_GAP, Z_TOP - TAB_T + 0.1))
        cap = cyl_z(x, y, CAP_D, Z_TOP - INLAY, Z_TOP)
        accents.append(cap)

    # Light pipe
    z_pipe = LED["top"] + PIPE_GAP
    pipe = cyl_z(LED["x"], LED["y"], PIPE_D, z_pipe, Z_TOP)
    body = body.cut(pipe)

    plane = cq.Plane(origin=(0, 0, Z_TOP), xDir=(1, 0, 0), normal=(0, 0, 1))
    accent = text_on(plane, ROOF_TEXT, INLAY)
    line = box(OX0 + LINE_INSET, OX1 - LINE_INSET, LINE_Y0, LINE_Y1, Z_TOP - INLAY, Z_TOP)
    accent = union_all([accent, line] + accents)
    body = body.cut(accent)
    return body, accent, pipe


def mesh_of(part, tol=0.02, ang=0.2):
    """Indexed triangle mesh, with the per-face duplicate vertices merged."""
    verts, tris = part.val().tessellate(tol, ang)
    index, out_v, out_t = {}, [], []
    remap = []
    for v in verts:
        key = (round(v.x, 5), round(v.y, 5), round(v.z, 5))
        if key not in index:
            index[key] = len(out_v)
            out_v.append(key)
        remap.append(index[key])
    for a, b, c in tris:
        t = (remap[a], remap[b], remap[c])
        if len(set(t)) == 3:
            out_t.append(t)
    return out_v, out_t


def place_lower(point):
    """Bottom on the bed."""
    x, y, z = point
    return (x - OX0 + PLATE_MARGIN_X, y - OY0 + PLATE_Y, z - Z_BOTTOM)


def place_upper(point):
    """Roof on the bed, turned over around the y axis."""
    x, y, z = point
    return (OX1 - x + PLATE_MARGIN_X + (OX1 - OX0) + PLATE_SPACING, y - OY0 + PLATE_Y, Z_TOP - z)


def write_3mf(path, objects):
    """Bambu Studio project: one object per shell, each filament a part of it.

    objects: [(name, [(part_name, filament, verts, tris), ...]), ...]
    """
    ns = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    body, comps, build, config = [], [], [], ['<?xml version="1.0" encoding="UTF-8"?>\n<config>']
    next_id = 1
    instances = []
    for name, parts in objects:
        ids = []
        for part_name, filament, verts, tris in parts:
            vtxt = "".join(f'<vertex x="{x:.5f}" y="{y:.5f}" z="{z:.5f}"/>' for x, y, z in verts)
            ttxt = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in tris)
            body.append(
                f'<object id="{next_id}" type="model"><mesh><vertices>{vtxt}</vertices>'
                f"<triangles>{ttxt}</triangles></mesh></object>"
            )
            ids.append((next_id, part_name, filament))
            next_id += 1
        comp = "".join(f'<component objectid="{i}"/>' for i, _, _ in ids)
        body.append(f'<object id="{next_id}" type="model"><components>{comp}</components></object>')
        build.append(f'<item objectid="{next_id}" printable="1"/>')
        config.append(
            f'<object id="{next_id}"><metadata key="name" value="{name}"/>'
            f'<metadata key="extruder" value="{ids[0][2]}"/>'
        )
        for i, part_name, filament in ids:
            config.append(
                f'<part id="{i}" subtype="normal_part"><metadata key="name" value="{part_name}"/>'
                f'<metadata key="extruder" value="{filament}"/></part>'
            )
        config.append("</object>")
        instances.append(next_id)
        next_id += 1
    # Bambu Studio reads the filament of each part from this file. No Application
    # tag in the model: tagged as its own project, the CLI crashes on loading it.
    config.append('<plate><metadata key="plater_id" value="1"/><metadata key="plater_name" value=""/>'
                  '<metadata key="locked" value="false"/>')
    for i in instances:
        config.append(f'<model_instance><metadata key="object_id" value="{i}"/>'
                      f'<metadata key="instance_id" value="0"/><metadata key="identify_id" value="{i}"/></model_instance>')
    config.append("</plate>")
    config.append("</config>")
    model = (
        f'<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" xmlns="{ns}">'
        f'<metadata name="Title">igiRadio v2 case</metadata><resources>{"".join(body)}</resources>'
        f'<build>{"".join(build)}</build></model>'
    )
    rels = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Target="/3D/3dmodel.model" Id="rel-1" '
        'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>'
    )
    types = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
        "</Types>"
    )
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", types)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)
        z.writestr("Metadata/model_settings.config", "\n".join(config))


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).parent)
    for sub in ("step", "stl"):
        (out / sub).mkdir(parents=True, exist_ok=True)
    lower, lower_accent = build_lower()
    upper, upper_accent, pipe = build_upper()
    parts = {
        "lower_body": lower,
        "lower_accent": lower_accent,
        "upper_body": upper,
        "upper_accent": upper_accent,
        "upper_light_pipe": pipe,
    }
    for name, part in parts.items():
        cq.exporters.export(part, str(out / "step" / f"{name}.step"))

    def placed(part, place):
        verts, tris = mesh_of(part)
        return [place(v) for v in verts], tris

    groups = [
        ("Guscio inferiore", place_lower, [("corpo", FIL_LOWER, lower), ("scritte", FIL_ACCENT, lower_accent)]),
        (
            "Guscio superiore",
            place_upper,
            [("corpo", FIL_UPPER, upper), ("scritte", FIL_ACCENT, upper_accent), ("guida luce", FIL_PIPE, pipe)],
        ),
    ]
    objects = []
    for name, place, group in groups:
        objects.append((name, [(n, f, *placed(p, place)) for n, f, p in group]))
    write_3mf(out / "igiradio_v2_case.3mf", objects)

    # Single-colour STLs in print orientation, with the inlays merged back
    for stem, place, group in [
        ("lower_shell", place_lower, [lower, lower_accent]),
        ("upper_shell", place_upper, [upper, upper_accent, pipe]),
    ]:
        merged = union_all(group)
        verts, tris = placed(merged, place)
        with open(out / "stl" / f"{stem}.stl", "w") as f:
            f.write(f"solid {stem}\n")
            for a, b, c in tris:
                f.write("facet normal 0 0 0\nouter loop\n")
                for i in (a, b, c):
                    f.write("vertex {:.5f} {:.5f} {:.5f}\n".format(*verts[i]))
                f.write("endloop\nendfacet\n")
            f.write(f"endsolid {stem}\n")

    print(json.dumps({
        "outer_mm": [round(OX1 - OX0, 2), round(OY1 - OY0, 2), round(Z_TOP - Z_BOTTOM, 2)],
        "z_bottom": Z_BOTTOM,
        "z_top": Z_TOP,
        "pocket_mm": [round(IX1 - IX0, 2), round(POCKET_Y1 - POCKET_Y0, 2), round(Z_BAT_TOP - Z_FLOOR_IN, 2)],
    }))


if __name__ == "__main__":
    main()
