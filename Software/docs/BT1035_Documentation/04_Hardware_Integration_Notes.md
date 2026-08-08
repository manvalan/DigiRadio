# Note Hardware per DigiRadio

## 1. Gestione Alimentazione
- **VCHG/VBAT**: Il modulo supporta ingressi da 3.0V a 4.2V.
- **SYS_CTRL**: Richiede un impulso di >20ms per avviare il power-up da stato DORMANT/OFF.

## 2. Integrazione ADAU1701 (DSP)
Se il DigiRadio utilizza l'ADAU1701 come processore audio:
- Collegare le uscite `SPK_P/N` del modulo agli ingressi analogici dell'ADAU1701 (tramite filtro passivo come descritto nel datasheet ADAU1701).
- In alternativa, utilizzare l'interfaccia I2S (Pin 4-7 del BT1035) per un collegamento digitale diretto, garantendo che i clock (BCLK, LRCLK) siano configurati in modalità coerente (BT1035 come Master/Slave a seconda del setup dell'ADAU).
