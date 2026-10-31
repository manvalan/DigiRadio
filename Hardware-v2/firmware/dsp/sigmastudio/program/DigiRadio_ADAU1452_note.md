# DigiRadio ADAU1452: note del progetto

Progetto SigmaStudio+ 3.4 per la batch2, equivalente a DigiRadioFinale-v2 (ADAU1701).
Compilato il 31/10/2026 con 0 errori e 1 avviso (vedi sotto). Non è mai stato scaricato su una scheda, e nessun ascolto è stato possibile.

Risorse usate: 706 parole su 8192 di PM (600 istruzioni), DM0 351 su 20480, DM1 184 su 20480.

## File
- `DigiRadio_ADAU1452.ssprj`: il progetto, si apre in SigmaStudio+.
- `build_digiradio_adau1452.py`: ricostruisce e compila il progetto da zero via scripting.
- `export/`: Export System Files. Sequenza di caricamento in `*_TxBuffer.dat`, `*_NumBytes.dat` e `*Schematic_0.h`; indirizzi in `*_PARAM.h`.
- `DigiRadio_ADAU1452/ADAU145xSchematic_0/`: uscita del compilatore (`Program.bin`, `compiler_output.log`, ...).

## Porte seriali (schema: Hardware-v2/kicad/audio-dommain.kicad_sch)
48 kHz, I2S, 2 canali, slot da 32 bit, parola da 24 bit, BCLK 64 fs (3,072 MHz). Cristallo 12,288 MHz, PLL a 294,912 MHz.

| Porta | Pin | Periferica | Clock | Canali DSP |
|---|---|---|---|---|
| SDATA_IN0 | 57, 58, 59 | Si4684 DOUT | **master**, il DSP pilota BCLK_IN0 e LRCLK_IN0 | 0, 1 |
| SDATA_IN1 | 60, 61, 62 | BT1058 I2S_OUT | slave dei pin IN1, pilotati da OUT0 | 16, 17 |
| SDATA_IN2 | 65, 66, 67 | ESP32 GPIO48 | slave dei pin IN2, pilotati da OUT1 | 32, 33 |
| SDATA_OUT0 | 39, 40, 41 | BT1058 I2S_IN | **master** | 0, 1 |
| SDATA_OUT1 | 42, 43, 44 | ESP32 GPIO42 | **master** | 16, 17 |
| S/PDIF TX | 5 | J5 PLT237 (TOSLINK) | interno | 0, 1 |

IN1 e IN2 non possono essere master: i loro pin sono sullo stesso net di BCLK_OUT0/LRCLK_OUT0 e BCLK_OUT1/LRCLK_OUT1, che già pilotano quei net. Il DSP genera comunque tutti i clock, e BT1058, ESP32 e Si4684 devono stare in slave. Registri confermati col datasheet Rev. D: 0xF204 = 0x2400 (slave da BCLK_IN1/LRCLK_IN1), 0xF208 = 0x4800 (slave da BCLK_IN2/LRCLK_IN2).

Sulla batch2 non c'è un DAC: SPK_* del BT1058 non sono collegati, quindi le uscite cablate sono BT1058, ESP32 e TOSLINK. IN3, OUT2 e OUT3 non hanno pin collegati e sono spente.

## Catena audio
Ingressi → blocco DC → selettore → compressore → guadagno +2 dB → EQ 6 bande → volume (−6 dB) → Super Bass → spread stereo → Dynamic Bass Boost L/R → limiter L/R → BT1058, ESP32, TOSLINK.

Selettore (`MOD_SOURCE_SELECT_INDEX_ADDR`): 0 radio, 1 Bluetooth, 2 beep, come nel firmware attuale; **3 ESP32 è nuovo**. Il default è 0, cioè radio.

## Confronto con il programma ADAU1701, verificato sul compilato
**Identici:** i 6 biquad dell'EQ; filtro, costante di tempo e frequenza del bass boost; tabella, RMS, hold e decay del compressore; limiter; beep a 440 Hz con splitter +8 dB; post gain +2 dB; volume −6 dB; AlphaSpread.

**Vicini ma non identici:**
- Spread: SpreadLevel 0,06288 contro 0,06291.
- Bass boost: stessa tabella nella forma (−5,92 dB piatti poi rampa lenta), ma ricalcolata dal blocco; ora ci sono due blocchi mono al posto di uno stereo, quindi i rilevatori L e R sono indipendenti.
- Compressore: la griglia del nuovo blocco parte da −153 dB; sotto −90 dB uso il primo valore della vecchia tabella. L'ultimo punto, sopra +3 dB, viene calcolato dal blocco.
- Super Bass: è un algoritmo diverso. Ho riportato crossover 500 Hz e intensità 1,3; BassGain resta al default 1,2. I parametri interni non corrispondono uno a uno e il suono **va valutato all'ascolto**.

**Tolti:** il guadagno unitario "Multiple 1".

## Da verificare sulla prima scheda
1. `SuperBass.Bypass_ON`: lasciato al default del blocco (compilato a 0). Che sia "effetto attivo" è un'ipotesi da confermare all'ascolto.
2. Con la sorgente Bluetooth l'audio torna verso il BT1058 da OUT0, e con l'ESP32 verso l'ESP32 da OUT1. Il firmware deve gestirlo, altrimenti possono esserci eco o ritorni.

## Avviso del compilatore
`Immediate load value '9395240960' is out of range`, riga 33 del compressore. Compare con tutti i compressori RMS della libreria ADI provati, indipendentemente dai parametri, anche con il blocco usato dall'esempio ufficiale ADAU1452. L'ho quindi trattato come un difetto della libreria, ma non so dire che istruzione tocchi: **va tenuto d'occhio al primo ascolto**.

## Particolarità dell'ambiente
Con Windows in italiano, lo scripting di SigmaStudio+ interpreta male i decimali (`UpdateNumericProperty` fa diventare 1.452 → 1452). Lo script usa `UpdatePropertiesV2` con il punto. Lo stesso motivo scrive `_CONVERT(0,5)` in `*_PARAM.h`; lo script lo corregge in `_CONVERT(0.5)`, mentre `*.param` conserva la virgola. In ogni caso il firmware dovrebbe usare i valori `_FIXPT`, che sono esadecimali.

## Celle per il firmware
Tutte in formato 8.24, scrittura con safeload. Gli indirizzi completi sono in `export/*_PARAM.h`. **Cambiano a ogni compilazione.**

| Funzione | Macro |
|---|---|
| Selettore sorgente | `MOD_SOURCE_SELECT_INDEX_ADDR` |
| Volume L/R | `MOD_MASTERVOL_L_GAIN_ADDR`, `MOD_MASTERVOL_R_GAIN_ADDR` |
| EQ banda n (0..5) | `MOD_EQ_STAGEn_B0_ADDR`, `_B1_`, `_B2_`, `_A1_`, `_A2_`; stessa convenzione di segno del vecchio programma; le bande dispari sono in DM1 |
| Bass boost | `MOD_BASSBOOST_L_*` e `MOD_BASSBOOST_R_*`: filtro in `FILTERA1/A2/B0/B1/B2`, tabella `TABLE` da 40 punti |
| Stereo spread | `MOD_STEREOSPREAD_SPREADLEVEL_ADDR`, `MOD_STEREOSPREAD_ALPHASPREAD_ADDR` |
| Beep | `MOD_BEEP_BEEPPRESSED_ADDR`, `MOD_BEEP_COS_ADDR` |
| Misuratori | `MOD_METER_{RADIO,BT,ESP32,OUT}_{L,R}_VALUE_ADDR`: livello letto via SPI, non serve più la Data Capture dell'ADAU1701 |

`setBassBoostLevel` del firmware attuale interpola verso identità un biquad e una tabella da 33 punti: qui la tabella ha 40 punti e i blocchi sono due.

## Licenza
I file esportati riportano la nota di Analog Devices sulla non distribuzione a terzi. Il repo pubblico li contiene già per l'ADAU1701; decidi tu se pubblicare anche questi.

## EEPROM e self-boot
**Immagine `adau1452_selfboot.bin`: non c'è.** SigmaStudio+ la genera solo scrivendo su una scheda via USBi, e le note di rilascio dicono che l'esportazione su file non è supportata. Il datasheet descrive header, blocchi e footer, ma non spiega come vengono impostati i registri né il campo di velocità EEPROM, e sul programma di SigmaStudio+ il formato è interno: un'immagine fatta a mano qui non sarebbe verificabile.

Strada proposta:
1. Finché non c'è, l'ESP32 carica il programma in RAM via SPI slave a ogni avvio.
2. Con la prima scheda: USBi cablato su SPI_ADAU, in SigmaStudio+ "Write Latest Compilation to E2PROM".
3. Il writer legge la EEPROM e salva l'immagine, che diventa `adau1452_selfboot.bin`.

**Writer `eeprom_writer/`:** MP0 SS, MP1 MOSI, MP2 SCLK uscite senza pull-up, MP3 MISO ingresso, porte audio spente, 48 kHz, 0 errori e 0 avvisi. Celle: `MOD_CELL_SS_DCVALUE_ADDR` 23, `MOD_CELL_MOSI_DCVALUE_ADDR` 22, `MOD_CELL_SCLK_DCVALUE_ADDR` 21, `MOD_CELL_MISO_VALUE_ADDR` 20. Le celle DC sono 8.24: alto = 0x01000000, basso = 0. Mai provato su hardware; **il valore che il pin vede sopra una certa soglia va verificato all'accensione**.