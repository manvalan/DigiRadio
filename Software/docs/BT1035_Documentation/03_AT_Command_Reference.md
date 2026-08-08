# Riferimento Rapido Comandi AT

## Comandi Generali
- `AT+VER`: Query versione firmware (es: `+VER=BT1035,V2.6.1...`).
- `AT+ADDR`: Lettura MAC Address (necessario per pairing forzato).
- `AT+STAT`: Query globale stati profili (DEV, SPP, GATT, HFP, A2DP, AVRCP).

## Audio e Connessione
- `AT+A2DPCONN=[MAC]`: Connessione a specifica sorgente.
- `AT+A2DPAUDIO=1`: Apre flusso audio.
- `AT+SPPSEND=[LEN],[DATA]`: Invio dati via SPP (max payload 236 byte).

## Eventi Asincroni
Il modulo risponde con eventi preceduti da `+`. Esempi:
- `+SPPDATA=[LEN],[PAYLOAD]`: Ricezione dati seriali.
- `+A2DPSTAT=4`: Streaming A2DP attivo.
