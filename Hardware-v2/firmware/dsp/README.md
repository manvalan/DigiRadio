# DSP outputs linked into the firmware

These files come from SigmaStudio+ and are picked up at build time. Each one is
optional: the firmware builds and boots without them and says what is missing.
They are git-ignored for now, because the exports carry an Analog Devices notice
against redistribution and this repo is public.

| File | Made with | Used for |
|---|---|---|
| `adau1452_program.bin` | `tools/sigma_export.py` on Export System Files | RAM download over SPI when the EEPROM is blank or self boot panics |
| `adau1452_cells.inc` | `tools/sigma_export.py` | Parameter addresses. Without it, no audio setting can be applied |
| `adau1452_selfboot.bin` | EEPROM image, as raw bytes | Written to the 25AA1024 at first boot |
| `adau1452_eeprom_writer.bin` | `tools/sigma_export.py --no-cells --name ...` on the writer project | Program that lets the ESP32 reach the 25AA1024 |
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

- Source mux: 0 radio, 1 BT1058, 2 beep, 3 ESP32. `core::ActiveSource::Bluetooth`
  means the ESP32 stream, as on v1, so it maps to 3. Nothing selects input 1 yet:
  with it the audio would go back to the BT1058 through OUT0.
- EQ band 0 is a fixed high-pass the firmware does not touch, as on v1.
- `tools/sigma_export.py` stops if a block's cells are no longer contiguous in the
  order the driver writes them, or if the L and R bass boost curves differ.
