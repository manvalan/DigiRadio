# Mapping Risorse DigiRadio

Analisi dei file del progetto DigiRadio:
- `DigiRadio_IC_1.h`: Mappatura dei registri del controller per il modulo IC.
- `DigiRadio_IC_1_REG.h`: Definizione dei registri di configurazione (probabilmente relativi alla configurazione I2C del modulo Bluetooth).
- `DigiRadio.xml`: Descriptor di sistema utilizzato dal software di configurazione/test del progetto DigiRadio.

Il modulo BT1035 deve essere mappato nell'indirizzo di registro 0x68 (default I2C) se il controller agisce come master I2C sul bus del modulo. Verificare che il pin `ADDRO` sia configurato correttamente in base alla netlist fornita.
