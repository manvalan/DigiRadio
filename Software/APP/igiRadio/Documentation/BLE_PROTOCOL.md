# BLE_PROTOCOL — igiRadio

## Riepilogo

**DigiRadio non espone un protocollo BLE GATT documentato per il controllo radio/audio.**

Il BLE dell'ESP32-S3 è usato **solo** per il provisioning Wi‑Fi (setup iniziale).

---

## BLE Wi‑Fi Provisioning (documentato)

| Parametro | Valore |
|-----------|--------|
| Stack | ESP-IDF `wifi_provisioning` + `scheme_ble` |
| Sicurezza | protocomm **Security0** (firmware 0.9.0; manuale `ch-api.tex` indica Security1 + PoP) |
| Proof of possession | **Non richiesto** nel firmware attuale |
| Nome advertising | `DigiRadio-<suffix>` (es. `DigiRadio-CC4DB4`) |
| Riferimento firmware | `components/net/src/BleProvisioning.cpp` |
| Riferimento manuale | `ch-api.tex` §BLE provisioning |

### Flusso

1. Dispositivo in setup mode (no credenziali Wi‑Fi salvate)
2. Advertise BLE con nome = SoftAP SSID
3. App iOS invia credenziali Wi‑Fi via protocomm (Security0, senza PoP)
4. Firmware valida, salva in NVS, reboot in STA
5. App passa a controllo via **HTTP** su LAN

### Implementazione igiRadio

- Usare **CoreBluetooth** solo per questa fase
- Compatibile con app generiche "ESP BLE Provisioning" (stesso protocollo Espressif)
- PoP: **non usato** nel firmware 0.9.0 (Security0)

### UNKNOWN

- UUID servizio/caratteristica custom DigiRadio per controllo: **non documentati**
- Notifiche BLE stato tuner: **non documentate**

---

## Controllo dispositivo (post-provisioning)

Vedi `APP_ARCHITECTURE.md` — trasporto **HTTP REST** porta 80.
