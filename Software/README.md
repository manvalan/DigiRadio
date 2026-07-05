# DigiRadio — Firmware

> ⚠️ **Status: in active development.**
> The firmware will be released here after hardware bring-up and validation on the
> first prototype. Publishing firmware before it can be tested on real hardware
> would not be meaningful, so this directory currently documents the **planned
> architecture** only.

---

## Target Platform

- **Host MCU:** Espressif **ESP32-S3** (native USB, Wi-Fi, BLE)
- **Toolchain:** ESP-IDF (planned)
- **DSP tooling:** Analog Devices **SigmaStudio** for the ADAU1701 audio flow

---

## Planned Architecture

The ESP32-S3 is the system host and orchestrates the three audio subsystems over
three separate buses.

### Boot sequence
1. ESP32-S3 releases the ADAU1701 reset line (GPIO-controlled).
2. ESP32-S3 loads the compiled SigmaStudio program into the DSP program/parameter
   RAM over **I²C** (host-load model; on-board self-boot EEPROM is a DNP option).
3. ESP32-S3 starts the DSP core.
4. ESP32-S3 loads the Si4684 firmware/patch over **SPI**.
5. FSC-BT1035 is brought up over **UART** (Feasycom ASCII command set).

### Runtime control
- **Audio parameters** (volume, EQ, source mix): written to the ADAU1701
  **safeload registers** over I²C to avoid audio pops.
- **Tuner:** DAB/FM band and station control via Si4684 over SPI.
- **Bluetooth:** A2DP source control (aptX Adaptive), pairing and status via
  FSC-BT1035 UART commands.
- **Connectivity:** optional Wi-Fi internet-radio stream injected into the DSP via
  a second I²S input.

---

## Bus Map (summary)

| Bus | Devices | Notes |
|---|---|---|
| **I²C** | ADAU1701, MAC EEPROM, (self-boot EEPROM DNP) | single system bus; distinct addresses |
| **SPI** | Si4684 | firmware/patch load + control |
| **UART** | FSC-BT1035 | ASCII command set, 4-wire with flow control |
| **I²S** | Si4684 → DSP → BT1035 (+ ESP32 in) | DSP is I²S master (48 kHz) |

See the [Technical Reference Manual](../docs/DigiRadio_Manual.pdf) for the complete
pin assignment and address map.

---

## Roadmap

- [ ] ESP32-S3 project skeleton (ESP-IDF)
- [ ] ADAU1701 program loader (SigmaStudio export → I²C write)
- [ ] Si4684 firmware loader + DAB/FM control
- [ ] FSC-BT1035 UART driver (A2DP source, aptX Adaptive)
- [ ] Runtime control (volume / EQ / source) via safeload
- [ ] Wi-Fi internet-radio source (optional)
- [ ] Bring-up notes and validated example configuration

---

## License

Firmware in this directory is licensed under the [MIT License](../LICENSE) once
released.
