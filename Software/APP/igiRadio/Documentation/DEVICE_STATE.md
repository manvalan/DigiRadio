# DEVICE_STATE — DigiRadioState

Modello centrale in `Models/DigiRadioState.swift`.

## Sezioni

| Sezione | Campi documentati |
|---------|-------------------|
| `connection` | host, isConnected, lastError |
| `health` | status, firmware, serialNumber, chips |
| `tuner` | band, locked, volume, fm?, dab? |
| `audio` | mixer, master, eq, enhancements |
| `bluetooth` | booted, pairing, a2dpState, deviceName, speaker |
| `stations` | [Station] |
| `streaming` | enabled, url |

Campi non documentati nell'API **non** sono presenti nel modello.
