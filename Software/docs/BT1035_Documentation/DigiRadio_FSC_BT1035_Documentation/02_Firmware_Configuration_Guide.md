# Guida alla Configurazione Firmware

## 1. Analisi file DigiRadio.params
Il file `DigiRadio.params` analizzato suggerisce parametri di inizializzazione specifici per l'interfacciamento del modulo. Durante il boot, il controller DigiRadio deve:
1. Inviare `AT+VER` per identificare la versione del modulo (es. V2.6.1).
2. Caricare i profili: `AT+PROFILE=341` (combinazione SPP, GATT Server, GATT Client, A2DP Source, ecc.).
3. Configurare la modalità I2S se il flusso audio è digitale (`AT+I2SCFG`):
   - Impostare `Param=67` per I2S Slave, 48kHz, 32-bit (o come richiesto dall'ADAU1701).

## 2. Gestione Profili
Il modulo supporta le topologie miste. È possibile mantenere simultaneamente una connessione A2DP (Audio Source) e una GATT (Data). 
- Nota: Per il passaggio tra le modalità, interrogare sempre lo stato tramite `AT+STAT` prima di tentare nuove connessioni.
