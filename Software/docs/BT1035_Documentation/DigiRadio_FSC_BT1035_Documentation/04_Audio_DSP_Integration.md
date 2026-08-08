# Integrazione DSP ADAU1701

Il collegamento BT1035 -> ADAU1701 può avvenire in due modi:
1. **Digitale (I2S)**: 
   - BT1035 (Master/Slave) -> SDATA_IN0 (ADAU1701).
   - Configurazione ADAU: Impostare `MP4/MP5` come ingressi I2S (LRCLK/BCLK).
   - Assicurarsi che il sample rate del modulo Bluetooth (`AT+I2SCFG`) corrisponda al clock configurato nell'ADAU tramite il software SigmaStudio.
   
2. **Analogico**:
   - Uscite differenziali BT1035 (`SPK_P/N`) -> ingressi ADC `ADC0/1`.
   - Utilizzare il filtro passivo di ricostruzione consigliato dal datasheet dell'ADAU1701 (50kHz corner) per evitare aliasing dovuto al campionamento del DAC interno del Bluetooth.
