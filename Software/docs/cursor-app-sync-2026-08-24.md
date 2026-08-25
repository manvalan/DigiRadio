# Note per l'app companion (igiRadio) — sync 2026-08-24

Riepilogo dei cambi firmware di oggi rilevanti per l'app iOS. Incollare in Cursor come contesto.

## 1. Nuovo endpoint: codec A2DP Bluetooth

```
GET  /api/bluetooth/a2dp-codec
POST /api/bluetooth/a2dp-codec   body: {"codec_mask": <0-63>}
```

- `GET` risponde `{"codec":"sbc"}` (valori possibili: `sbc`, `aptx`, `aptx-hd`, `aptx-ll`, `aptx-adaptive`; se il modulo BT1035 negozia AAC la risposta può non essere interpretabile — il datasheet Feasycom non documenta un codice di ritorno per AAC, quindi in quel caso l'app deve gestire un valore/errore sconosciuto senza andare in crash).
- `POST` con `codec_mask` è una bitmask: `BIT0`=AAC, `BIT1`=aptX, `BIT2`=aptX-LL, `BIT3`=aptX-HD, `BIT4`=aptX-Adaptive, `BIT5`=LDAC. `0` forza solo SBC (baseline obbligatorio). Risposta `{"status":"saved"}` o `{"status":"error","reason":"..."}`.
- **Importante**: il cambio vale solo per la prossima negoziazione — se un dispositivo Bluetooth è già connesso, serve chiamare `POST /api/bluetooth/disconnect` e far riconnettere il device (dal lato telefono/speaker) perché il nuovo codec venga effettivamente usato. Se l'app espone questa funzione in UI, considerare di mostrare un messaggio tipo "riconnetti il dispositivo Bluetooth per applicare".

## 2. Fix: salvataggio profilo audio (EQ/mixer) ora persiste davvero

Prima di oggi, `PUT /api/audio/profile` applicava le modifiche dal vivo ma **falliva sempre** silenziosamente nel salvataggio permanente (bug: nome chiave NVS troppo lungo). L'app probabilmente vedeva errori intermittenti o impostazioni che sparivano al riavvio della scheda. Ora è risolto e verificato: le modifiche sopravvivono a un riavvio. Nessun cambio di formato richiesto lato app — stesso schema JSON di sempre.

## 3. Fix: il parser JSON del firmware ora tollera JSON non-compatto

Prima di oggi il parser lato firmware richiedeva JSON strettamente compatto (`{"key":"value"}`, **nessuno spazio** dopo i due punti) — un `JSONEncoder` Swift in modalità non-compatta (es. con `.prettyPrinted`, o formattazione di default in alcune configurazioni) poteva produrre `"key": "value"` e far fallire il parsing lato server con `invalid_json` o `missing_field`.

Questo è ora risolto per tutti gli endpoint (tuner, audio profile, stazioni, bluetooth, wifi, streaming, DSP param). **Se nell'app c'era un workaround per forzare JSON compatto** (es. `JSONEncoder().outputFormatting` impostato esplicitamente senza spazi, o costruzione manuale di stringhe JSON), non è più necessario ma può restare — è comunque compatibile.

## 4. Nomi campi corretti (promemoria, riscontrati oggi durante i test manuali)

Attenzione a questi nomi campo esatti attesi dal firmware — un nome sbagliato produce `missing_field`, non necessariamente un errore chiaro:

- `POST /api/tuner/tune` per DAB: `{"band":"dab","freq_index":<0-37>}` — **non** `"frequency"`.
- `POST /api/tuner/tune` per FM: `{"band":"fm","frequency_khz":<khz>}`.
- `POST /api/stations` per una stazione FM: `{"name":"...","band":"fm","fm_frequency_khz":<khz>}` — **non** `"frequency_khz"` a livello radice.
- `POST /api/stations` per una stazione DAB: `{"name":"...","band":"dab","dab_freq_index":<0-37>}` (opzionali `dab_service_id`, `dab_component_id`).
- `POST /api/stations/remove`: `{"index":<n>}` — **non** `{"name":"..."}`.

Se l'app usa nomi diversi da questi in qualche punto, verificare contro `components/core/src/StationListJson.cpp` e `components/core/src/TunerJson.cpp` nel repo firmware (fonte di verità).

## 5. Nessun cambio di schema per mixer/EQ

Lo schema di `GET|PUT /api/audio/profile` è invariato:

```json
{
  "mixer": {
    "si4684_left_db": 0, "si4684_right_db": 0,
    "esp32_left_db": -96, "esp32_right_db": -96,
    "mix_left_db": 0, "mix_right_db": -96
  },
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
}
```

Nota: `esp32_left_db`/`esp32_right_db`/`mix_right_db` a `-96` = leg ESP32 (streaming web radio) mutato, `si4684_*`/`mix_left_db` a `0` = leg radio (FM/DAB) aperto — è il default "radio-first" di fabbrica. Se l'app vuole passare a streaming web-radio senza sentire anche la radio in sovrapposizione, deve invertire questi gain (radio a `-96`, esp32 a `0`) — non è automatico solo abilitando `POST /api/streaming`.

## 6. Bug noto, non ancora risolto

- `PUT /api/audio/profile` con `enhancements` (stereo/bass) diverso da zero **sovrascrive** eventuali modifiche manuali dell'EQ — bug già tracciato, non toccato oggi.
- La banda EQ indice 0 (20 Hz) è in realtà un filtro passa-alto fisso non modificabile dal DSP — il valore `gain_db` mostrato/inviato per quella banda è cosmetico, non ha effetto reale sul suono.
