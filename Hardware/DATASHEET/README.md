# Component datasheets (DigiRadio hardware)

Authoritative vendor PDFs used for schematic/PCB design review and firmware
bring-up. Paths are relative to this repository root (`DigiRadio/`).

| File | Component | Use in DigiRadio |
|------|-----------|------------------|
| `SI4684-A10.pdf` | Silicon Labs Si4684-A10 | RSTB power sequencing, SPI framing, supply levels, SMODE |
| `adau1701.pdf` | Analog Devices ADAU1701 | I²C pull-ups (2 kΩ R1/R16 on PCB; datasheet 2.2 kΩ), MCLK = 256×fS, serial-port master/slave loopback (MP10→MP4, MP11→MP5) |
| `AN851.pdf` | Silicon Labs AN851 | Si4684 bypass capacitors, antenna layout, schematic guidance |
| `FSC-BT1035_Datasheet_EN.pdf` | Feasycom FSC-BT1035 | Module pinout, electrical and mechanical specs (v1.0) |
| `FSC-BT1035_programming_user_guide_1.1.1.pdf` | Feasycom FSC-BT1035 | **AT command reference** — firmware `core::Bt1035At*` maps to §5/§6 |

## Firmware cross-reference

| Manual section | Primary datasheet |
|----------------|-------------------|
| `ch-hardware.tex` §validation | `SI4684-A10.pdf`, `AN851.pdf`, `adau1701.pdf`, BT1035 datasheets |
| `ch-bt1035.tex` §AT commands | `FSC-BT1035_programming_user_guide_1.1.1.pdf` §5 |

When extending BT1035 commands, cite the programming guide section in code comments
and add a host test in `Software/components/core/test/bt1035_at_test.cpp`.
