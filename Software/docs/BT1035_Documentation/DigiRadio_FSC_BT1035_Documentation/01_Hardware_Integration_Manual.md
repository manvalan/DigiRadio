# Manuale di Integrazione Hardware FSC-BT1035

## 1. Architettura di Connessione
Il modulo FSC-BT1035 deve essere integrato seguendo rigorosamente le indicazioni di Feasycom per evitare degrado del segnale RF:
- **Pin 34 (SYS_CTRL)**: Richiede un segnale di accensione (>20ms). È consigliato un circuito di controllo tramite il controller DigiRadio.
- **Pin 16 (BT_RTS/PIO2)**: Utilizzato nativamente per il MUTE degli stadi di potenza. Integrare questa funzione nel sistema di amplificazione del DigiRadio.
- **Audio I/O**: Le uscite `SPK_P/N` sono differenziali. Per il collegamento al DSP ADAU1701:
    - Utilizzare uno stadio di ingresso con op-amp (es. AD8608 come da datasheet ADAU1701) per convertire il segnale differenziale in singolo (o mantenere il bilanciamento se l'ingresso DSP lo permette).
    - Impedenza: Assicurarsi di rispettare i filtri RC per il passaggio basso (corner 50kHz) per eliminare il noise del codec Bluetooth.

## 2. Note di Layout (Critiche)
- **Clearance Antenna**: Rispettare rigorosamente l'area di "Keep Out" (5mm clearance) attorno all'antenna PCB integrata, come indicato nel documento `FSC-BT1035 Datasheet` (sezione 9).
- **Grounding**: Utilizzare una via stitching densa attorno al modulo per connettere i piani di massa e prevenire leakage RF nel PCB del DigiRadio.
