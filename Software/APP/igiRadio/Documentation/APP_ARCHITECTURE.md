# APP_ARCHITECTURE — igiRadio

## Stack

```
SwiftUI Views
    ↓ @Observable ViewModels
DigiRadioService (protocol)
    ↓
RealDigiRadioService ──→ HTTPDigiRadioClient (URLSession)
MockDigiRadioService   ──→ dati in-memory

BLEProvisioningService (solo setup Wi‑Fi)
    ↓ CoreBluetooth + protocomm (ESP Security1)
```

## Principi

1. Le View **non** chiamano URLSession o CoreBluetooth direttamente.
2. Un solo `DigiRadioState` osservabile aggiornato dal service.
3. Errori tipizzati (`DigiRadioError`).
4. Dependency injection via `AppEnvironment` in `igiRadioApp`.

## Connessione

| Fase | Meccanismo |
|------|------------|
| Prima configurazione | BLE provisioning **oppure** join SoftAP + POST /api/wifi |
| Uso normale | mDNS `digiradio-xxxxxx.local` o IP manuale |
| Base URL | `http://{host}/api/...` |

## Polling vs push

- Nessuna notifica BLE documentata per stato tuner.
- `TunerViewModel` può pollare `GET /api/tuner/status` a intervallo configurabile quando la schermata radio è attiva.
