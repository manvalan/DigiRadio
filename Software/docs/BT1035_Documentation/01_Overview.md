# Documentazione Integrazione FSC-BT1035 - Sistema DigiRadio

## 1. Introduzione
Il modulo FSC-BT1035 (basato su chipset Qualcomm QCC3056) è un modulo Bluetooth Dual-Mode. Nell'ecosistema DigiRadio, agisce come bridge wireless per l'audio e i dati seriali (SPP/GATT).

## 2. Architettura di Sistema
Il modulo si collega al controller principale DigiRadio tramite:
- **UART (Baudrate 115200 8N1)**: Comando e controllo tramite AT Commands.
- **Audio Interface**: Uscita differenziale (SPK_P/N) per il sistema di amplificazione o I2S per processori DSP esterni (come l'ADAU1701 presente nel vostro sistema).
- **Controllo Hardware**: Pin `SYS_CTRL` per il Power-On e `RESET` per il riavvio hardware.
