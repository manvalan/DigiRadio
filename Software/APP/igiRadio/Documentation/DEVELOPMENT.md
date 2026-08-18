# DEVELOPMENT — igiRadio

## Requisiti

- macOS con **Xcode 15+** (testato Xcode 26.6)
- iOS 17+ deployment target
- Dispositivo DigiRadio su stessa rete Wi‑Fi (STA) o in setup SoftAP

## Aprire il progetto

```bash
open /Users/michelebigi/Documents/Develop/DigiRadio/Software/APP/igiRadio/igiRadio.xcodeproj
```

Rigenerare il progetto (se necessario):

```bash
cd Software/APP/igiRadio
python3 Scripts/generate_xcodeproj.py
```

## Build e test da terminale

```bash
cd Software/APP/igiRadio
xcodebuild -scheme igiRadio -destination 'platform=iOS Simulator,name=iPhone 17' build test
```

## Scheme

- **igiRadio** — app principale
- **igiRadioTests** — unit test protocollo e state

## Mock vs reale

In **DEBUG**, `AppEnvironment` avvia in modalità mock (`MockDigiRadioService`).  
Disattivare in **Connessione → Modalità demo (Mock)** e inserire l'host HTTP reale.

## Test su hardware

1. Provisioning Wi‑Fi (BLE discovery in-app, o app ESP BLE Provisioning; oppure SoftAP `http://192.168.4.1`)
2. In app: Impostazioni → Connessione → host `http://digiradio-<suffix>.local`
3. Verificare `GET /api/health`

## Documentazione firmware

`/Users/michelebigi/Documents/Develop/DigiRadio/Software/docs/manual/ch-api.tex`

## Architettura runtime

```
SwiftUI → ViewModel → DigiRadioService
  ├── RealDigiRadioService → HTTPDigiRadioClient (REST JSON)
  └── MockDigiRadioService
BLEProvisioningService (CoreBluetooth, solo discovery setup)
```

Il controllo tuner/audio **non** usa GATT proprietario — vedi `BLE_PROTOCOL.md`.
