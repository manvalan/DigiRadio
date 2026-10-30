PROJECT NAME
DigiRadio v2 – battery DAB+/FM radio with SigmaDSP audio, Bluetooth and optical S/PDIF

CATEGORY
Open Theme

KEYWORDS
DAB+, FM radio, Si4684, ESP32-S31, ADAU1452, SigmaDSP, Bluetooth audio, S/PDIF, BQ25896, KiCad, 6-layer PCB, BoardLab

COVER PHOTO
cover-4x3.png

PROJECT STORY
DigiRadio is an open-source portable digital radio designed in KiCad 10: DAB, DAB+ and FM reception, a SigmaDSP audio hub, Bluetooth audio, an optical S/PDIF output and a Li-ion battery that charges from USB-C. Everything sits on one 6-layer board of 89 × 187 mm.

How it works
- Radio: a Silicon Labs Si4684 receives DAB/DAB+ and FM from an SMA antenna jack, with its own 19.2 MHz crystal and SPI flash for its firmware.
- Control: an Espressif ESP32-S31-WROOM-3 module runs the firmware, controls the radio and the DSP and carries audio over I²S. It has 128 Mbit of external flash and a USB-C port.
- Audio: an Analog Devices ADAU1452 SigmaDSP is the audio hub. It takes three I²S inputs (radio, Bluetooth, ESP32), sends audio back to the Bluetooth module and the ESP32, and drives an optical S/PDIF output. It boots from a 1 Mbit SPI EEPROM.
- Bluetooth: a Feasycom FSC-BT1058 module on its own filtered 3.3 V rail, controlled by the ESP32 over UART.
- Power: a TI BQ25896 charges the battery from USB-C and a MAX17048 fuel gauge reports the charge. Two TPS63031 buck-boost converters make the 3.3 V digital and audio rails, a TPS7A20 LDO makes 1.8 V for the radio and a TLV62568 buck makes 1.2 V for the DSP core.

Checked before fab with BoardLab
Before ordering, the KiCad project was checked with BoardLab (https://boardlab.it), a design-review service that reads the schematic, the layout and the datasheet of every IC and tells you whether the board can be ordered, with the evidence behind each finding. The review caught real issues on earlier revisions, such as missing I²C pull-ups, crystal load and drive level, and the DSP PLL filter, and every finding cites the datasheet page it comes from. The full BoardLab report is attached.

Images: render-3d.png, layout-top.png
Schematic: DigiRadio-v2-schematic.pdf

LINK TO SOURCE CODE
https://github.com/manvalan/DigiRadio

LINK TO DOCUMENTATION
https://github.com/manvalan/DigiRadio (README, schematic PDF, BoardLab report)

WHO I AM
Michele Bigi, electronics designer and the author of BoardLab.

PROJECT DESCRIPTION / FILES
KiCad project, schematic PDF, Gerber and drill files, BOM, BoardLab report.
Gerber download: allowed. License: CERN-OHL-S-2.0 (same as the repository).
