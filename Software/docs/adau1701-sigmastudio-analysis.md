# ADAU1701 SigmaStudio firmware — analisi completa registri, funzioni e API

Analisi del programma compilato in `Firmware/ADAU1701-Firmware/` (`DigiRadio_IC_1.h`,
`DigiRadio_IC_1_PARAM.h`, `DigiRadio_IC_1_REG.h`, `DigiRadio_NetList.xml`), incrociato
con il driver firmware (`components/drivers/adau1701/`) e le API HTTP
(`components/net/src/SetupWebServer.cpp`). Ogni valore di indirizzo/registro citato è
letto direttamente dai file di export SigmaStudio, non inventato.

**Root cause del bug "volume/mixer/EQ non si salvano mai" trovato e risolto in questa
sessione (2026-08-24): vedi §7.** Tutto il resto del documento descrive l'architettura
del programma DSP com'è oggi, dopo il fix.

---

## 1. Catena del segnale (da `DigiRadio_NetList.xml`)

```
                    ┌─────────────┐
Si4684 (I2S) ──L──▶ │ Gain Si4674 │──▶┐
              ──R──▶ │  (addr 3,4) │──▶┤
                    └─────────────┘   │
                                      │   ┌──────────────┐   ┌──────────────┐
ESP32 (I2S)  ──L──▶ ┌─────────────┐   ├──▶│  St Mixer1   │──▶│  Param EQ1   │──┐
              ──R──▶ │ Gain ESP32  │──▶┤   │ ST0/ST1/ST2  │   │  6 bande     │  │
                    │  (addr 5,6) │──▶┘   │ (addr 9-11)  │   │ (addr 12-41) │  │
                    └─────────────┘       └──────────────┘   └──────────────┘  │
                                                                                │
Beep1 (interno) ──▶ Single1 ──▶ SSplitter1 ──▶ (ST2, ingresso mixer)           │
 (addr 0-2)          (addr 7)     (addr 8)                                     │
                                                                                ▼
                                                              ┌──────────────────┐
                                                              │  Multiple1 (L/R)  │
                                                              │  = MASTER VOLUME  │
                                                              │   (addr 42, 43)   │
                                                              └──────────────────┘
                                                                     │
                                                        ┌────────────┴────────────┐
                                                        ▼                         ▼
                                                ┌───────────────┐        ┌───────────────┐
                                                │  Limiter1 (L) │        │  Limiter2 (R) │
                                                │  (addr 44-58) │        │  (addr 59-73) │
                                                └───────────────┘        └───────────────┘
                                                        │                         │
                                                        ▼                         ▼
                                                    Output1                   Output2
                                                (→ BT1035 I2S, L)         (→ BT1035 I2S, R)
```

Punto architetturale importante, spesso fonte di confusione: **ogni sorgente ha DUE
livelli di gain in cascata**, non uno solo:

1. **Gain pre-mixer** (`SI4674`/`SI4674_1` addr 3/4, `ESP32`/`ESP32_1` addr 5/6) — un
   gain indipendente per canale L e R, applicato PRIMA che il segnale entri nel mixer.
2. **Gain del mixer** (`STMIXER1_ST0_VOLUME`/`ST1_VOLUME` addr 9/10) — un SOLO gain per
   l'intera coppia stereo di quella sorgente, applicato AL mixer.

Se uno dei due è a -96dB (muto) e l'altro è aperto, il risultato finale è comunque
muto — impostare solo uno dei due senza sapere dell'altro è la causa più comune di
"ho cambiato il volume ma non sento niente".

---

## 2. Mappa completa dei registri Parameter RAM (74 indirizzi, 0-73)

| Indirizzo | Nome SigmaStudio | Funzione | Gruppo |
|---|---|---|---|
| 0 | `BEEP1_ENABLE` | Abilita generatore toni interno | Diagnostica |
| 1 | `BEEP1_KICK` | Trigger del generatore toni | Diagnostica |
| 2 | `BEEP1_BEEP_FREQ` | Frequenza del tono (fissa, mai cambiata a runtime) | Diagnostica |
| 3 | `SI4674` | Gain pre-mixer Si4684, canale L | Mixer |
| 4 | `SI4674_1` | Gain pre-mixer Si4684, canale R | Mixer |
| 5 | `ESP32` | Gain pre-mixer streaming ESP32, canale L | Mixer |
| 6 | `ESP32_1` | Gain pre-mixer streaming ESP32, canale R | Mixer |
| 7 | `SINGLE1` | Gain del tono Beep prima dello splitter | Diagnostica |
| 8 | `SSPLITTER1` | Splitter mono→stereo per il tono Beep | Diagnostica |
| 9 | `STMIXER1_ST0_VOLUME` | Gain mixer, leg Si4684 (stereo, un solo controllo) | Mixer |
| 10 | `STMIXER1_ST1_VOLUME` | Gain mixer, leg ESP32 (stereo, un solo controllo) | Mixer |
| 11 | `STMIXER1_ST2_VOLUME` | Gain mixer, leg Beep — **mai esposto da nessuna API curata**, fisso a 1.0 (unity) di fabbrica | Mixer (non controllato) |
| 12-16 | `PARAMEQ1_ST0_*` (B0,B1,B2,A0,A1) | Banda EQ 0 — **filtro passa-alto fisso, mai scritto da `applyEq()`** | EQ (banda bloccata) |
| 17-21 | `PARAMEQ1_ST1_*` | Banda EQ 1 (default 100 Hz) | EQ |
| 22-26 | `PARAMEQ1_ST2_*` | Banda EQ 2 (default 400 Hz) | EQ |
| 27-31 | `PARAMEQ1_ST3_*` | Banda EQ 3 (default 1000 Hz) | EQ |
| 32-36 | `PARAMEQ1_ST4_*` | Banda EQ 4 (default 3000 Hz) | EQ |
| 37-41 | `PARAMEQ1_ST5_*` | Banda EQ 5 (default 8000 Hz) | EQ |
| 42 | `MULTIPLE1` | **Volume master**, canale L | Master |
| 43 | `MULTIPLE1_1` | **Volume master**, canale R | Master |
| 44-58 | `LIMITER1_*` | Limiter canale L (soglia a indirizzo 55) | Limiter |
| 59-73 | `LIMITER2_*` | Limiter canale R (soglia a indirizzo 70) | Limiter |

Le 6 bande EQ condividono gli **stessi coefficienti per L e R** (il cell "Parametric
EQ - Double Precision" è stereo ma con un solo set di coefficienti per banda) — non
esiste un controllo EQ separato per canale.

Formato dati: fixed-point **5.23** (28 bit significativi, non 32 — un errore di
decodifica di questo formato durante l'indagine del 23/08 aveva fatto sembrare
instabile un filtro che in realtà era corretto; vedi `core::floatToFixpoint823()` /
sign-extend da bit 27 in `Adau1701Driver.cpp`).

---

## 3. Registri di controllo (Core Control block, 0x800-0x827)

| Indirizzo | Nome | Uso nel nostro firmware |
|---|---|---|
| 0x81C | Core Control Register | Scritto una sola volta al boot dal replay del programma compilato |
| 0x81E | Serial Output Control | Compilato, mai modificato a runtime |
| 0x81F | Serial Input Control (bit IBP/ILP) | **Storia**: un override IBP=1 qui era stato introdotto il 16/08 insieme a un fix non correlato di Si4684, mai validato da solo, e causa del fruscio multi-giorno risolto oggi (24/08). Rimosso — ora resta al default compilato (0x00) |
| 0x820/0x821 | MP Config 0/1 | Compilato, mai toccato |
| 0x822 | Analog Power-Down | Compilato, mai toccato |
| 0x826 | Analog Interface Register 2 | Contiene i bit IBIAS_ADJ/VREF_TRIM di trim analogico interno, mai toccati; è un registro diverso da quello dove viveva l'override IBP (0x81F) |

---

## 4. API HTTP e corrispondenza con i registri — tutte testate dal vivo il 24/08

| Endpoint | Metodo | Corpo | Registri toccati | Persistenza NVS | Testato |
|---|---|---|---|---|---|
| `/api/audio/profile` | GET | — | legge tutto | — | ✅ |
| `/api/audio/profile` | PUT | mixer + master + eq[6] + enhancements | 3,4,5,6,9,10,12-41,42,43 | ✅ | ✅ (round-trip completo dopo riavvio) |
| `/api/audio/reset` | POST | — | ripristina `AudioProfile::factoryDefault()` (mixer: **entrambe** le sorgenti aperte a 0dB, non radio-first) | ✅ | ✅ |
| `/api/audio/stereo-enhance` | POST | `{"level":0-100}` | bande EQ 3,4,5 (sovrascrive) | ✅ | ✅ |
| `/api/audio/bass-enhance` | POST | `{"level":0-100}` | bande EQ 1,2 (sovrascrive) | ✅ | ✅ |
| `/api/audio/beep` | POST | `{"enabled":true/false}` | 0 (`BEEP1_ENABLE`), 1 (`BEEP1_KICK`) | ❌ (live-only, come da design) | ✅ |
| `/api/dsp/params` | GET | — | elenco statico nome→indirizzo (74 celle) | — | ✅ |
| `/api/dsp/param` | PUT | `{"name":"...","value":<float>}` | qualunque cella per nome, incluse quelle **non** raggiungibili dalle API curate (es. `STMIXER1_ST2_VOLUME`) | ❌ (live-only, "escape hatch" come SigmaStudio Remote Connection) | ✅ (scritto e ripristinato `STMIXER1_ST2_VOLUME`) |
| `/api/tuner/xtal-calibrate` | POST | `{"ibias","ctun","xtal_freq_hz"}` | Si4684 POWER_UP (non ADAU1701) | ✅ (dal fix di stasera) | ✅ |

### Campo `"locked"` per banda EQ (aggiunto oggi)

`GET/PUT /api/audio/profile` ora include `"locked":true/false` per ciascuna delle 6
bande in `eq[]`:
- banda 0: **sempre `true`** — è il passa-alto fisso, qualsiasi valore scritto è
  cosmetico, non ha mai effetto udibile (`Adau1701Driver::applyEq()` la salta
  esplicitamente).
- bande 1,2: `true` quando `bass_level > 0` — l'enhancement le sovrascrive con valori
  calcolati da formula, un edit manuale in quel momento non è udibile.
- bande 3,4,5: `true` quando `stereo_level > 0`, stesso motivo.

Prima di questo fix il comportamento era identico (le bande erano già sovrascritte),
ma l'API non lo segnalava — sembrava che "il salvataggio non facesse nulla" quando in
realtà stava facendo esattamente quello che il DSP prevede, solo senza dirlo.

---

## 5. Bug trovati e stato

| # | Sintomo | Causa reale | Stato |
|---|---|---|---|
| 1 | Mixer/volume/EQ salvati non sopravvivono MAI a un riavvio | **NVS inizializzato DOPO** `AudioService::loadAndApply()` in `app_main()` — ogni lettura falliva con `ESP_ERR_NVS_NOT_INITIALIZED` (0x1101), silenziosamente, da sempre | ✅ risolto oggi — riordinato boot in `main.cpp`, verificato dal vivo |
| 2 | Anche dopo il fix #1, mixer e master tornavano comunque al preset "radio" a ogni riavvio | `HardwareBootstrap::boot()` chiamava `applyRadioFirstMix()` **incondizionatamente** dopo il caricamento, sovrascrivendo qualunque valore appena ripristinato da NVS | ✅ risolto oggi — ora condizionale, si applica solo se non c'è un profilo salvato |
| 3 | Modificare la banda EQ 0 (20 Hz) via web UI/app non ha mai effetto udibile | È il filtro passa-alto fisso del programma compilato, `applyEq()` la salta di proposito | ✅ ora segnalato via `"locked":true`, non più silenzioso |
| 4 | Attivare bass/stereo enhancement sembra "cancellare" le modifiche manuali dell'EQ sulle bande coinvolte | `core::applyEnhancementsToEq()` sovrascrive bande 1-2 (bass) e 3-5 (stereo) con valori a formula quando il livello è > 0 | ✅ ora segnalato via `"locked":true` sulle bande coinvolte; il comportamento DSP resta invariato (per design) |
| 5 | Il leg Beep del mixer (`STMIXER1_ST2_VOLUME`, indirizzo 11) non è mai regolabile dalle API curate | Nessun bug — è un ingresso diagnostico interno, mai stato nei piani esporlo. Raggiungibile comunque via `/api/dsp/param` se serve per debug | Non è un bug, solo documentato |
| 6 | Fruscio persistente su radio e streaming (multi-giorno) | Override IBP=1 su `SerialInputRegister` (0x81F), introdotto l'8/16 agosto insieme a un fix non correlato, mai validato da solo — condiviso da tutti gli ingressi seriali del chip | ✅ risolto ieri (24/08, prima parte della sessione) — vedi `docs/si4684-rf-investigation-report.md` |

---

## 6. Come testare manualmente (comandi usati stasera)

```bash
# Stato attuale
curl http://192.168.1.62/api/audio/profile

# Scrivi un pattern distintivo (mixer + master + 5 bande EQ)
curl -X PUT http://192.168.1.62/api/audio/profile -d '{
  "mixer": {"si4684_left_db": 0, "si4684_right_db": 0,
            "esp32_left_db": -96, "esp32_right_db": -96,
            "mix_left_db": 0, "mix_right_db": -96},
  "master": {"left_db": -4, "right_db": -4},
  "eq": [
    {"gain_db": 0, "center_hz": 20, "q": 1.414},
    {"gain_db": 5, "center_hz": 100, "q": 1},
    {"gain_db": -3, "center_hz": 400, "q": 1.2},
    {"gain_db": 2, "center_hz": 1000, "q": 1},
    {"gain_db": -1, "center_hz": 3000, "q": 1},
    {"gain_db": 4, "center_hz": 8000, "q": 1}
  ],
  "enhancements": {"stereo_level": 0, "bass_level": 0}
}'

# Riavvia la scheda (fisicamente, o via reset seriale) e rileggi:
curl http://192.168.1.62/api/audio/profile
# → deve mostrare ESATTAMENTE gli stessi valori scritti sopra
```

Per leggere/scrivere una singola cella arbitraria (debug avanzato, non persistente):

```bash
curl http://192.168.1.62/api/dsp/params        # elenco nome→indirizzo
curl -X PUT http://192.168.1.62/api/dsp/param -d '{"name":"STMIXER1_ST2_VOLUME","value":1.0}'
```

---

## 7. Riferimenti file

| File | Contenuto |
|---|---|
| `Firmware/ADAU1701-Firmware/DigiRadio_IC_1_PARAM.h` | Sorgente di verità per tutti gli indirizzi Parameter RAM |
| `Firmware/ADAU1701-Firmware/DigiRadio_NetList.xml` | Topologia del segnale (chi è collegato a chi) |
| `components/drivers/adau1701/include/adau1701/Adau1701ParamTable.hpp` | Tabella nome→indirizzo generata da PARAM.h, usata da `/api/dsp/param` |
| `components/drivers/adau1701/include/adau1701/Adau1701ParamMap.hpp` | Funzioni helper indirizzo-per-banda-EQ e indirizzo-per-sorgente-mixer |
| `components/drivers/adau1701/src/Adau1701Driver.cpp` | Implementazione `applyMixer`/`applyEq`/`setMasterVolume`/`setBeepEnabled` |
| `components/services/audio/src/AudioService.cpp` | `loadAndApply()`, `applyRadioFirstMix()`, enhancement plumbing |
| `components/core/src/EnhancementsDesign.cpp` | Formula di sovrascrittura EQ per bass/stereo enhancement |
| `main/main.cpp` | Ordine di boot (`app_main()`) — sede del fix root-cause di oggi |
| `main/hardware_bootstrap.cpp` | `HardwareBootstrap::boot()` — sede del secondo fix (radio-first condizionale) |
