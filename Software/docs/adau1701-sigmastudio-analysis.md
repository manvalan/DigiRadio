# ADAU1701 SigmaStudio firmware — analisi completa registri, funzioni e API

Analisi del programma compilato in `Firmware/ADAU1701-Firmware/` (`DigiRadio_IC_1.h`,
`DigiRadio_IC_1_PARAM.h`, `DigiRadio_IC_1_REG.h`, `DigiRadio_NetList.xml`), incrociato
con il driver firmware (`components/drivers/adau1701/`) e le API HTTP
(`components/net/src/SetupWebServer.cpp`). Ogni valore di indirizzo/registro citato è
letto direttamente dai file di export SigmaStudio, non inventato.

**Questo documento descrive la revisione "DigiRadioFinale" (2026-08-25)**, che
sostituisce il programma DSP precedente (74 parametri, mixer additivo) con uno
molto più ricco (224 parametri) basato su un **selettore di sorgente esclusivo**
invece di un mixer. §5 raccoglie anche i bug storici della revisione precedente,
per riferimento.

---

## 1. Catena del segnale (da `DigiRadio_NetList.xml`)

```
Input1 (Si4684 L/R, ESP32 L/R) ──┬─▶ DCB1 (L radio) ─┐
                                  ├─▶ DCB2 (R radio) ─┼─▶ Compressor1 ─▶ 1×RTA1 (L) ─┐
                                  ├─▶ DCB4 (L esp32) ─┤   (stereo RMS)   1×RTA2 (R) ─┤
                                  └─▶ DCB3 (R esp32) ─┘                 1×RTA3 (L) ─┤   (readback,
                                                                        1×RTA4 (R) ─┤    ingresso)
Beep1 ─▶ S Splitter1 (mono→stereo) ─────────────────────────────────────────────────┤
                                                                                     ▼
                                                                        ┌──────────────────┐
                                                                        │   MX1 (mux)      │
                                                                        │ select = DC1     │
                                                                        │ 0=Radio 1=BT      │
                                                                        │ 2=Beep (raw int!) │
                                                                        └──────────────────┘
                                                                                     │
                                                                                     ▼
                                                                        Param EQ1 (6 bande, addr 59-88)
                                                                                     │
                                                                                     ▼
                                                                        SPhat1 (Spatializer, addr 89-140)
                                                                                     │
                                                                                     ▼
                                                                        Gen Filter1 (Voice Clarifier,
                                                                          addr 141-145 — non tarato)
                                                                                     │
                                                                                     ▼
                                                                        Multiple1 (Master Volume,
                                                                          addr 146-147)
                                                                                     │
                                                                                     ▼
                                                                        Bass Boost1 (Dynamic Bass Boost,
                                                                          addr 148-187)
                                                                                     │
                                                                          ┌──────────┴──────────┐
                                                                          ▼                     ▼
                                                                  1×RTA1_2 (readback)   1×RTA2_2 (readback)
                                                                          │                     │
                                                                          ▼                     ▼
                                                                    Limiter1 (L)          Limiter2 (R)
                                                                    addr 209-223          addr 194-208
                                                                          │                     │
                                                                          ▼                     ▼
                                                                      Output1               Output2
                                                                  (→ BT1035 I2S, L)     (→ BT1035 I2S, R)
```

**Cambio architetturale rispetto alla revisione precedente**: non esiste più un
mixer che combina Si4684 + ESP32 + Beep simultaneamente. `MX1` è un **selettore
esclusivo**: passa UNA SOLA delle tre coppie stereo alla volta, scelta scrivendo
`DC1` (indirizzo 3). Non esistono più gain pre-mixer per sorgente (`SI4674`/`ESP32`
sono spariti) — il livello si controlla solo con Master Volume ed EQ, a valle
della selezione.

**Scoperta empirica cruciale (2026-08-25)**: `DC1` è dichiarato
`SIGMASTUDIOTYPE_FIXPOINT` nell'export compilato (che farebbe pensare al formato
5.23 usato da tutte le altre celle), ma **MX1 legge invece un intero a 32 bit
grezzo**. Scrivere `0x00800000` (= 1.0 in 5.23) non cambia la sorgente; scrivere
`0x00000001` sì. Confermato dal vivo: 0=Radio, 1=Bluetooth(ESP32), 2=Beep. Vedi
`adau1701::paramSourceIndex()` e `Adau1701Driver::selectSource()`.

---

## 2. Mappa completa dei registri Parameter RAM (224 indirizzi, 0-223)

| Indirizzi | Blocco | Funzione | Controllo runtime |
|---|---|---|---|
| 0-2 | `BEEP1_*` | Tono di test interno | `setBeepEnabled()` (live-only) |
| 3 | `DC1` | Selettore sorgente per MX1 — **intero grezzo**, non 5.23 | `selectSource()` |
| 4-7 | `DCB1-4_POLE` | DC blocker, uno per canale (radio L/R, esp32 L/R) | Fisso (compilato) |
| 8 | `SSPLITTER1` | Fan-out mono→stereo per Beep | Fisso |
| 9-46 | `COMPRESSOR1_*` | Compressore RMS stereo (curva a 34 punti) su radio L/R | Fisso |
| 47-58 | `1XRTA3,4,1,2` | 4 VU-meter in ingresso (radio L/R, esp32 L/R) | Lettura via indirizzo speciale 2074 — **meccanismo non ancora implementato in firmware** |
| 59-88 | `PARAMEQ1_ST0..5_*` | EQ parametrico, 6 bande × 5 coefficienti (B0,B1,B2,A0,A1) | `setEqBand()`/`applyEq()` |
| 89-140 | `SPHAT1_*` | Spatializer (SuperPhat) — solo `SPREAD1`/`SPREAD2` (139,140) sono la manopola di intensità; il resto (filtro crossover, tabella compander) è tarato in SigmaStudio | `setStereoSpreadLevel()` — scala SPREAD1/2 proporzionalmente (0=nessuno spread aggiunto, 100=pieno) |
| 141-145 | `GENFILTER1_ST0_*` | Voice Clarifier — **ancora piatto/identità** in questo export (b0=1, resto 0), non tarato | Non ancora implementato |
| 146-147 | `MULTIPLE1`, `MULTIPLE1_1` | Master Volume L/R | `setMasterVolume()` |
| 148-187 | `BASSBOOST1_*` | Dynamic Bass Boost — `BASSFREQUENCY`/`TIMECONSTANT` fissi; filtro crossover (B0,B1,B2,A1,A2) e tabella compander a 33 punti (`TABLE0-32`) sono la parte "intensità" | `setBassBoostLevel()` — scala filtro+tabella verso l'identità/unità (0=bypass piatto, 100=pieno compilato) |
| 188-193 | `1XRTA2_2,1_2` | 2 VU-meter in uscita (post Bass Boost, pre limiter) | Stesso meccanismo di lettura non ancora implementato |
| 194-208 | `LIMITER2_*` | Limiter canale R (soglia indirizzo 205) | Fisso |
| 209-223 | `LIMITER1_*` | Limiter canale L (soglia indirizzo 220) | Fisso |

Formato dati: fixed-point **5.23** per quasi tutte le celle (28 bit significativi,
sign-extend da bit 27 — vedi `core::floatToFixpoint823()`), **eccetto `DC1`** che è
un intero grezzo a 32 bit (vedi sopra).

---

## 3. API HTTP e corrispondenza con i registri

| Endpoint | Metodo | Corpo | Cosa tocca | Persistenza | Testato dal vivo |
|---|---|---|---|---|---|
| `/api/audio/profile` | GET/PUT | `active_source`, `master`, `eq[6]`, `enhancements` | `DC1`, EQ 59-88, Master 146-147, Bass Boost/Spatializer via enhancements | ✅ | ✅ |
| `/api/audio/reset` | POST | — | `AudioProfile::factoryDefault()` (Radio, EQ piatto, enhancements 0) | ✅ | — |
| `/api/audio/stereo-enhance` | POST | `{"level":0-100}` | `SPHAT1_SPREAD1/2` (scala proporzionale) | ✅ | ✅ (effetto soggettivo/sottile) |
| `/api/audio/bass-enhance` | POST | `{"level":0-100}` | `BASSBOOST1_*` filtro+tabella (scala verso identità) | ✅ | ✅ (confermato su radio, non su tono fisso — l'algoritmo è dinamico, reagisce a contenuto con dinamica reale) |
| `/api/audio/beep` | POST | `{"enabled":true/false}` | `BEEP1_ENABLE/KICK` | ❌ (live-only, per design) | ✅ |
| `/api/dsp/params` | GET | — | elenco statico nome→indirizzo (230 celle, incluse le 6 speciali readback) | — | ✅ |
| `/api/dsp/param` | PUT | `{"name":"...","value":<float>}` | qualunque cella per nome — **eccetto `DC1`, che qui va scritto già come intero puro, non tramite `selectSource()`** | ❌ (live-only) | ✅ (sweep completo di 223/224 celle scrivibili, 0 errori) |

`active_source` accetta `"radio"`, `"bluetooth"`, `"beep"`. Il vecchio campo
`"mixer"` (con `si4684_left_db`/`esp32_left_db`/`mix_left_db` ecc.) **non esiste
più** — non ha più senso dato che non c'è più un mixer da bilanciare.

Il campo `"locked"` per banda EQ (introdotto il 24/08 quando bass/stereo enhance
sovrascrivevano le bande EQ) **ora è sempre `false` tranne la banda 0** (passa-alto
fisso): bass/stereo enhance non toccano più l'EQ, guidano direttamente Bass
Boost1/SPhat1.

---

## 4. VU-meter (readback) — non ancora implementato

Le 6 celle `1×RTA*` sono veri VU-meter (level detector) posizionati in 4 punti
d'ingresso e 2 punti d'uscita. Il loro valore letto NON sta al normale indirizzo
Parameter RAM di ciascuna — condividono tutte l'indirizzo speciale **2074**, con
un codice di selezione diverso per ciascuna (`VALUES_1XRTA1` = 0x0376, ecc., tipo
`SIGMASTUDIOTYPE_SPECIAL`/`SIGMASTUDIOTYPE_10_14`). Questo è il meccanismo di
"Data Capture" dell'ADAU1701 (registro indiretto), diverso dal semplice
read/safeload usato ovunque altrove in questo firmware. **Va ancora reverse-
engineerato e implementato** — non è stato inventato né testato in questa sessione.

---

## 5. Bug trovati e stato (cronologia)

| # | Sintomo | Causa reale | Stato |
|---|---|---|---|
| 1 | Mixer/volume/EQ salvati non sopravvivono MAI a un riavvio (revisione precedente) | NVS inizializzato dopo `AudioService::loadAndApply()` in `app_main()` | ✅ risolto 2026-08-24 |
| 2 | Mixer e master tornavano al preset "radio" a ogni riavvio anche dopo il fix #1 | `HardwareBootstrap::boot()` chiamava `applyRadioFirstMix()` incondizionatamente | ✅ risolto 2026-08-24 |
| 3 | Qualsiasi modifica EQ/enhancement diversa da zero → silenzio totale (anche sul tono di test) | Segno mancante in `core::designPeakingEq()`: i coefficienti di retroazione RBJ (sottrattivi) venivano scritti tali e quali nei registri A0/A1 dell'ADAU1701 (additivi) — retroazione positiva invece che negativa, il filtro divergeva su un valore costante (DC, inaudibile) | ✅ risolto 2026-08-25 — vedi `components/core/src/BiquadDesign.cpp`, test di stabilità in `biquad_design_test.cpp` |
| 4 | Bridge TCP SigmaStudio (porta 8086): `accept()` in loop infinito su EBADF da ogni boot | `SigmaStudioTcpServer::stop()` cancellava incondizionatamente il singleton del socket attivo, anche quando chiamato sull'oggetto move-from (ogni boot ne crea uno) | ✅ risolto 2026-08-25 |
| 5 | Nuovo netlist DigiRadioFinale: nessun mixer, solo `MX1` a selezione esclusiva | Non è un bug — cambio di design intenzionale, confermato dall'utente | Documentato, non un bug |
| 6 | Scrivere `DC1` in formato 5.23 (es. 1.0 = 0x00800000) non cambia la sorgente selezionata | `MX1` legge `DC1` come intero grezzo, non fixpoint, nonostante `TYPE_DC1` compilato dica `SIGMASTUDIOTYPE_FIXPOINT` | ✅ risolto 2026-08-25 — confermato dal vivo (0/1/2 → Radio/BT/Beep) |
| 7 | Fruscio persistente su radio e streaming (multi-giorno, revisione precedente) | Override IBP=1 su `SerialInputRegister` (0x81F), mai validato in isolamento | ✅ risolto 2026-08-24 |

---

## 6. Come testare manualmente

```bash
# Stato attuale
curl http://192.168.1.62/api/audio/profile

# Cambia sorgente
curl -X PUT http://192.168.1.62/api/audio/profile -d '{
  "active_source": "bluetooth",
  "master": {"left_db": 0, "right_db": 0},
  "eq": [
    {"gain_db": 0, "center_hz": 20, "q": 1.414},
    {"gain_db": 0, "center_hz": 100, "q": 1},
    {"gain_db": 0, "center_hz": 400, "q": 1},
    {"gain_db": 0, "center_hz": 1000, "q": 1},
    {"gain_db": 0, "center_hz": 3000, "q": 1},
    {"gain_db": 0, "center_hz": 8000, "q": 1}
  ],
  "enhancements": {"stereo_level": 0, "bass_level": 0}
}'

# Bass Boost / Spatializer
curl -X POST http://192.168.1.62/api/audio/bass-enhance -d '{"level":100}'
curl -X POST http://192.168.1.62/api/audio/stereo-enhance -d '{"level":100}'
```

Per leggere/scrivere una singola cella arbitraria (debug avanzato, non
persistente — **non usare per `DC1`**, che ha una semantica intero-grezzo
diversa da tutte le altre celle):

```bash
curl http://192.168.1.62/api/dsp/params        # elenco nome→indirizzo (230 celle)
curl -X PUT http://192.168.1.62/api/dsp/param -d '{"name":"SPHAT1_SPREAD1","value":0.0629}'
```

---

## 7. Riferimenti file

| File | Contenuto |
|---|---|
| `Firmware/ADAU1701-Firmware/DigiRadio_IC_1_PARAM.h` | Sorgente di verità per tutti gli indirizzi Parameter RAM (export DigiRadioFinale) |
| `Firmware/ADAU1701-Firmware/DigiRadio_NetList.xml` | Topologia del segnale (chi è collegato a chi) |
| `components/core/include/core/ActiveSource.hpp` | Enum Radio/Bluetooth/Beep + nota sulla semantica intero-grezzo di DC1 |
| `components/drivers/adau1701/include/adau1701/Adau1701ParamTable.hpp` | Tabella nome→indirizzo generata da PARAM.h (230 celle) |
| `components/drivers/adau1701/include/adau1701/Adau1701ParamMap.hpp` | `paramAddrEqBandBase()`, `paramSourceIndex()` |
| `components/drivers/adau1701/src/Adau1701Driver.cpp` | `selectSource()`, `applyEq()`, `setMasterVolume()`, `setBassBoostLevel()`, `setStereoSpreadLevel()`, `setBeepEnabled()` |
| `components/core/src/BiquadDesign.cpp` | `designPeakingEq()` — fix del segno A0/A1 (2026-08-25) |
| `components/services/audio/src/AudioService.cpp` | `loadAndApply()`, `applyRadioFirstMix()`, `selectSource()`, enhancement plumbing |
| `docs/manual/ch-sigmastudio.tex` §"Next revision: DigiRadioFinale" | Diagramma e tabella indirizzi per il manuale PDF |
