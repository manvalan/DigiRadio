# Configurazione e Setup del Modulo

## 1. Parametri di Default
- **Nome BR/EDR**: FSC-BT1035
- **Nome LE**: FSC-BT1035-LE
- **Baudrate**: 115200

## 2. Inizializzazione via UART
Al boot, inviare i comandi per impostare l'ambiente:
1. `AT+RESTORE`: (Opzionale) Ripristina impostazioni di fabbrica.
2. `AT+PROFILE=BITMASK`: Selezionare i profili necessari (Somma i valori binari: SPP=1, GATT_S=2, A2DP_S=32). Esempio: `AT+PROFILE=33` (SPP+A2DP).
3. `AT+NAME=DigiRadio_V1,0`: Imposta nome dispositivo senza suffisso MAC.
4. `AT+REBOOT`: Applicare i cambiamenti.

## 3. Verifica stato
Monitorare il pin `LEDO` (Pin 17) che indica lo stato di pairing (onda quadra durante scanning, alto a connessione stabilita).
