# Deep Dive Comandi AT e Gestione Eventi

## 1. Protocollo Comunicazione
Il modulo risponde in formato ASCII. Ogni stringa inizia con `
` (0x0D 0x0A).
- Esempio Parsing:
  Se il controller riceve `+SPPDATA=10,ABCDEFGHIJ`, il parser deve estrarre il valore `10` come lunghezza e gestire il buffer dei successivi 10 caratteri.

## 2. Throughput Mode (TPMODE)
Per applicazioni ad alto volume dati (es. aggiornamento firmware del DigiRadio via Bluetooth):
1. Inviare `AT+TPMODE=1`.
2. A questo punto, il modulo cessa di interpretare i comandi AT e inoltra ogni byte ricevuto sulla UART direttamente nel link radio (SPP/GATT).
3. Per tornare in modalità comando, è necessario inviare una sequenza di escape definita nel firmware o resettare il modulo.
