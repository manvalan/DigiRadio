# DSP outputs linked into the firmware

These files come from SigmaStudio+ and are picked up at build time. Each one is
optional: the firmware builds and boots without them and says what is missing.

| File | Made with | Used for |
|---|---|---|
| `adau1452_program.bin` | `tools/sigma_export.py` on Export System Files | RAM download over SPI when the EEPROM is blank or self boot panics |
| `adau1452_cells.inc` | `tools/sigma_export.py --cells` | Parameter addresses. Without it, no audio setting can be applied |
| `adau1452_selfboot.bin` | EEPROM image, as raw bytes | Written to the 25AA1024 at first boot |
| `adau1452_eeprom_writer.bin` | `tools/sigma_export.py --name` on the writer project | Program that lets the ESP32 reach the 25AA1024 |
| `adau1452_eeprom_writer_cells.inc` | By hand from its `_PARAM.h` | `kCellSs`, `kCellMosi`, `kCellSclk`, `kCellMiso` |

## Writer program

The ESP32 cannot drive the DSP's master SPI pins through registers. The data
sheet says an MPx output carries what the DSP program sends it, updated once per
sample. So the writer is a SigmaStudio+ project with four things:

- MP0, MP1, MP2 as general-purpose outputs without pull-up, each fed from a DC cell.
- MP3 as a general-purpose input, copied to a readable cell.
- The master port disabled, so the pins stay in multipurpose mode.
- At least 48 kHz, ideally 192 kHz, since each pin change takes one sample.

If SigmaStudio+ can export its own EEPROM writer instead, that is better, and
`Adau1452Eeprom.cpp` changes accordingly.

## Program contract

- Source mux index: 0 radio, 1 ESP32, 2 beep (`core::ActiveSource`).
- EQ band 0 is a fixed high-pass the firmware does not touch, as on v1.
- `kBassAmount` and `kStereoAmount` take 0.0 to 1.0. v1 blended the compiled
  curves in the firmware. If v2 keeps those blocks as they are, the driver has
  to do the same with the new compiled values.
