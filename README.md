<div align="center">

# DigiRadio

### DAB+/FM Bluetooth Receiver with SigmaDSP Audio Processing

**Open-source digital radio — Si4684 tuner · ADAU1701 SigmaDSP · Bluetooth aptX Adaptive · ESP32-S3**

![Status](https://img.shields.io/badge/hardware-verified-brightgreen)
![Firmware](https://img.shields.io/badge/firmware-in%20development-orange)
![PCB](https://img.shields.io/badge/PCB-6--layer-blue)
![Hardware License](https://img.shields.io/badge/hardware-CERN--OHL--S-lightgrey)
[![Firmware License: Apache 2.0](https://img.shields.io/badge/Firmware%20License-Apache%202.0-blue.svg)](Software/LICENSE)
</div>

---

## Overview

**DigiRadio** is an open-source digital radio receiver that combines terrestrial
broadcast reception (**DAB+ / DAB / FM**) with a programmable audio-processing
stage and high-quality **Bluetooth** audio output.

Incoming broadcast audio is decoded by a Silicon Labs **Si4684** digital-radio
receiver, processed by an Analog Devices **ADAU1701 SigmaDSP** (equalisation,
mixing, level control), and transmitted over Bluetooth by a Qualcomm QCC3056-based
**Feasycom FSC-BT1035** module supporting the **aptX Adaptive** codec. An Espressif
**ESP32-S3** acts as the host controller, orchestrating the three subsystems and
providing Wi-Fi/BLE connectivity and a USB service interface.

The entire tuner → DSP → Bluetooth path is **fully digital (I²S, 48 kHz / 24-bit)** —
no analogue conversions are introduced between the tuner and the wireless link.

```
 FM/DAB antenna → [ Si4684 ] --I²S--> [ ADAU1701 DSP ] --I²S--> [ FSC-BT1035 ] → aptX
                       ↑                     ↑                        ↑
                       └──── SPI ──── [ ESP32-S3 host ] ──── UART ────┘
                                             │  I²C
```

---

## Key Features

- **DAB+ / DAB / FM** reception (Si4684) via external whip antenna (SMA)
- **Fully digital audio path** — no avoidable A/D–D/A conversions
- **Programmable audio processing** on the ADAU1701 SigmaDSP
- **Bluetooth 5.2** output with **aptX / aptX HD / aptX Adaptive**
- **ESP32-S3** host with native USB, Wi-Fi and BLE
- **USB-C powered**; single-rail 3.3 V + low-noise 1.8 V for the tuner
- **6-layer impedance-controlled PCB**, 50 × 90 mm, two ground planes
- Three antennas (ESP32 2.4 GHz, BT1035 2.4 GHz, FM/DAB SMA) with proper keep-outs

---

## Repository Structure

```
DigiRadio/
├── docs/                     → symlink to Software/docs/manual (canonical LaTeX manual)
│   ├── manual.tex            build with: cd docs && latexmk -lualatex manual.tex
│   └── manual.pdf
├── Hardware/
│   ├── schematics/           Schematic (PDF)
│   ├── gerber/               Gerber + drill files (fabrication)
│   ├── bom/                  Bill of Materials
│   ├── pick-and-place/       Component placement (CPL / centroid)
│   ├── easyeda/              EasyEDA source project
│   └── 3d/                   3D renders / STEP
└── Software/                 Firmware (in development — see Software/README.md)
```

---

## Documentation

The full design is described in the **[Technical Manual](docs/manual.pdf)**,
covering hardware, firmware architecture, companion-chip drivers (Si4684,
ADAU1701, FSC-BT1035), the HTTP JSON API, and build instructions. LaTeX
sources live in `Software/docs/manual/`; the repository root `docs/` entry
is a symbolic link to that folder (single source of truth).

---

## Design Highlights

| Topic | Choice | Rationale |
|---|---|---|
| **Audio quality** | QCC3056 / aptX Adaptive | The codec is the dominant quality factor on a wireless link; the DAC lives in the sink |
| **Audio path** | Fully digital I²S | Avoids unnecessary A/D–D/A conversions between tuner and Bluetooth |
| **I²S master** | ADAU1701 (12.288 MHz) | Clean ×256 → 48 kHz; avoids sample-rate mismatch |
| **Stack-up** | 6-layer, 2× GND | Solid ground reference under RF, crystals and USB |
| **USB routing** | TOP, ref. GND | 90 Ω diff achievable at manufacturable geometry |
| **Antennas** | 3 zones, diagonal 2.4 GHz | Coexistence separation + full-layer keep-outs |

---

## Status

- ✅ **Schematic** — complete and reviewed
- ✅ **PCB layout** — 6-layer, DRC clean, plane continuity verified
- ✅ **BOM** — finalised (manufacturable / sourced)
- 🔜 **Prototype** — in fabrication (PCBWay)
- 🛠️ **Firmware** — in active development (released after bring-up)

---

## Manufacturing

The board is manufactured with **PCBWay** as a 6-layer, impedance-controlled PCB
with turnkey assembly. The FSC-BT1035 Bluetooth module is sourced from Feasycom
(the footprint uses a BT806-compatible, pin-identical land pattern). See
[Chapter Hardware](docs/manual.pdf) and manufacturing notes in the manual for
fabrication settings and MSL-3 handling of the BT module.

---

## License
- **Hardware** (schematics, PCB, Gerbers, BOM): CERN-OHL-S v2 — see [`LICENSE`](LICENSE)
- **Firmware / Software**: Apache-2.0 — see [`Software/LICENSE`](Software/LICENSE)
- **Documentation**: CC BY 4.0

Intellectual property of the design remains with the author. You are free to study,
modify and build the hardware under the terms of the respective licenses.

---

## Author

**Michele Bigi** — Open-source hardware project.

*Contributions, issues and questions are welcome via the repository issue tracker.*
