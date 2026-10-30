# DigiRadio v2 (batch 2)

Hardware files for the second DigiRadio board. The first board lives in
[`../Hardware`](../Hardware).

The v2 board adds an ADAU1452 DSP. The KiCad project is in [`kicad/`](kicad/) and the
firmware for this board in [`firmware/`](firmware/).

Planned layout, matching `Hardware/`:

| Folder | Contents |
|---|---|
| `kicad/` | KiCad project (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, `.kicad_dru`) |
| `firmware/` | Firmware for the v2 board |
| `schematics/` | Exported schematic PDF, netlist, 3D STEP |
| `gerber/` | Fabrication outputs |
| `bom/` | Bill of materials |
| `pick-and-place/` | Assembly positions |
| `3D/` | Renders |
| `DATASHEET/` | Datasheets for parts new to v2 |
