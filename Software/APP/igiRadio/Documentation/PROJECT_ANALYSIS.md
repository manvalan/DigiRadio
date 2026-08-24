# PROJECT_ANALYSIS — DigiRadio (fonte: documentazione firmware)

**Data analisi:** 2026-08-18  
**Fonte primaria:** `Software/docs/manual/` (non modificata)  
**Firmware di riferimento:** 0.8.5+

---

## 1. Documenti analizzati

| File | Contenuto |
|------|-----------|
| `ch-intro.tex` | Introduzione prodotto |
| `ch-hardware.tex` | PCB, pinout, catena audio |
| `ch-firmware.tex` | Architettura firmware ESP32-S3 |
| `ch-si4684.tex` | Tuner FM/DAB, tune, seek, RDS, DLS |
| `ch-adau1701.tex` | DSP SigmaStudio, mixer, EQ, safeload |
| `ch-bt1035.tex` | Modulo BT classic A2DP (UART AT), I2S da ADAU |
| `ch-sigmastudio.tex` | Design SigmaStudio |
| `ch-api.tex` | **HTTP REST API** (protocollo app) |
| `ch-classes.tex` | Classi firmware |
| `ch-build.tex` | Build/flash |
| `ch-licensing.tex` | Licenze |

Documentazione aggiuntiva letta: `Software/CLAUDE.md`, `Software/docs/security-flash-nvs.md`, `BleProvisioning.cpp`.

---

## 2. Distinzione critica: BLE vs controllo

### Controllo dispositivo (tuner, audio, preset, BT config)

**Trasporto documentato:** HTTP JSON REST su **TCP porta 80** (Wi‑Fi).

- Setup mode: SoftAP `DigiRadio-<suffix>` → `http://192.168.4.1/`
- STA mode: stessa API su IP LAN o **mDNS** `digiradio-<suffix>.local`
- **NON esiste** un protocollo GATT proprietario documentato per tuner/audio/preset.

### BLE (ESP32-S3 onboard)

**Solo provisioning Wi‑Fi** in setup mode (`ch-api.tex` §BLE provisioning):

- Stack: ESP-IDF `wifi_provisioning` + `scheme_ble`
- Sicurezza: **Security1** (protocomm)
- **Proof of possession (PoP):** serial number dispositivo (stesso valore di `GET /api/health` → `serialNumber`)
- **Nome servizio BLE:** SSID SoftAP = `DigiRadio-<suffix>` (es. `DigiRadio-CC4DB4`)
- Su successo: credenziali salvate in NVS, reboot in STA
- **Non è un endpoint HTTP** — nessuna REST call

### Bluetooth Audio (streaming verso speaker esterno)

**Modulo separato FSC-BT1035** (classic BR/EDR A2DP), controllato dal firmware via **UART AT**, esposto all'app solo tramite **HTTP** `/api/bluetooth/*`.

L'iPhone **non** si collega in A2DP al DigiRadio per ascoltare: il DigiRadio invia audio al Bose/Speaker via BT1035.

---

## 3. Architettura hardware (sintesi)

```
Si4684 (SPI) ──I2S──► ADAU1701 (I2S master 48 kHz) ──I2S──► BT1035 ──A2DP──► Speaker
ESP32-S3 (Wi‑Fi/BLE, HTTP server, opz. I2S test)
```

Sorgenti audio nel DSP: **Si4684** (radio), **ESP32 I2S** (stream locale / phone push).

---

## 4. Endpoint HTTP documentati (controllo app)

| Metodo | Path | Funzione |
|--------|------|----------|
| GET | `/api/health` | Stato, fw, serial, chip boot |
| POST | `/api/wifi` | Provisioning STA (reboot) |
| POST | `/api/wifi/scan` | Scan reti vicine |
| GET | `/api/tuner/status` | Stato tuner FM/DAB |
| GET | `/api/tuner/services` | Lista servizi DAB |
| POST | `/api/tuner/tune` | Sintonia FM/DAB |
| POST | `/api/tuner/play` | Play servizio DAB |
| POST | `/api/tuner/seek` | Seek FM up/down |
| POST | `/api/tuner/scan` | Scan singola stazione |
| POST | `/api/tuner/scan/full` | Scan completo banda FM |
| GET/PUT | `/api/audio/profile` | Profilo mixer/EQ/master |
| POST | `/api/audio/reset` | Reset profilo factory |
| POST | `/api/audio/stereo-enhance` | Enhancement stereo 0–100 |
| POST | `/api/audio/bass-enhance` | Enhancement bass 0–100 |
| POST | `/api/audio/beep` | Beep diagnostico ADAU |
| GET | `/api/dsp/params` | Tabella parametri SigmaStudio |
| PUT | `/api/dsp/param` | Scrittura cella raw (live) |
| POST | `/api/dsp/program` | Upload blob DSP (reboot) |
| POST | `/api/system/ota` | OTA firmware ESP32 (reboot) |
| GET/POST | `/api/streaming` | Web radio MP3 URL |
| PUT | `/api/stream/phone` | Push PCM stereo 48 kHz da phone |
| GET | `/api/bluetooth/status` | Stato BT1035 |
| POST | `/api/bluetooth/pair` | Modalità discoverable |
| POST | `/api/bluetooth/pair/stop` | Esci pairing |
| POST | `/api/bluetooth/disconnect` | Disconnect A2DP |
| GET | `/api/bluetooth/paired` | Lista paired |
| POST | `/api/bluetooth/scan` | Scan speaker vicini |
| POST | `/api/bluetooth/connect` | Connect A2DP + opz. save |
| GET/POST/DELETE | `/api/bluetooth/speaker` | Speaker default |
| POST | `/api/bluetooth/reconnect` | Reconnect manuale |
| POST | `/api/bluetooth/auto-reconnect` | Retry count 0–15 |
| GET | `/api/stations` | Lista preset |
| POST | `/api/stations` | Aggiungi preset |
| POST | `/api/stations/remove` | Rimuovi preset |
| POST | `/api/stations/reorder` | Riordina preset |
| POST | `/api/stations/tune` | Richiama preset |

---

## 5. Funzionalità per schermata app

| Area | Supportato (documentato) | Note |
|------|--------------------------|------|
| Connessione Wi‑Fi | ✅ | BLE prov. + SoftAP + POST /api/wifi |
| FM tune/seek/scan | ✅ | RSSI, SNR, RDS PS/RT |
| DAB tune/play/services | ✅ | DLS, ensemble, service list |
| Preset | ✅ | CRUD + reorder + recall |
| Volume/mixer/EQ | ✅ | Via audio profile |
| DSP raw params | ✅ | Escape hatch tecnico |
| BT speaker pairing | ✅ | Via HTTP → BT1035 AT |
| OTA firmware | ✅ | POST /api/system/ota |
| OTA DSP | ✅ | POST /api/dsp/program |
| Phone audio stream | ✅ | PUT /api/stream/phone |
| Web radio stream | ✅ | POST /api/streaming |
| Artwork album | ❌ | **UNKNOWN** — non in API |
| Batteria dispositivo | ❌ | Alimentato da rete |
| AUX/USB come sorgente UI | ❌ | Non esposto in API |
| Notifiche push BLE stato | ❌ | Nessun GATT documentato |

---

## 6. Implicazioni per igiRadio

1. **DigiRadioService** → implementazione **HTTP REST** (URLSession), non GATT custom.
2. **BLE layer** → solo **Wi‑Fi provisioning** (ESP protocomm Security1 + PoP = serial).
3. **MockDigiRadioService** → stessa interfaccia, dati fittizi.
4. **Discovery** → mDNS `.local`, IP manuale, o join SoftAP in setup.
5. **Real-time** → polling selettivo su `/api/tuner/status` quando in radio; nessuna notifica BLE documentata.

---

## 7. UNKNOWN — specification required

| Voce | Motivo |
|------|--------|
| UUID GATT controllo tuner/audio | Non documentati — controllo è HTTP |
| Artwork/metadata streaming | Non in API tuner |
| Protocol version field | Solo `fw` in health |
| Hardware revision API | Solo `serialNumber` + chip flags |
