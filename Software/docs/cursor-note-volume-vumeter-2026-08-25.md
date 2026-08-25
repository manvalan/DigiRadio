# Nota per Cursor — Volume master e VU-meter

Due punti precisi da correggere/implementare nell'app. Segui esattamente i
contratti sotto, non improvvisare formati diversi.

Dispositivo di test: `http://192.168.1.62`.

---

## 1. Volume master

**Endpoint**: fa parte del profilo audio completo, non ha una rotta a sé.

```
GET  /api/audio/profile   -> legge lo stato attuale (incluso "master")
PUT  /api/audio/profile   -> scrive il profilo COMPLETO
```

Campo:

```json
"master": {"left_db": 0, "right_db": 0}
```

Regole obbligatorie:

1. **Range reale: da -96.0 a +12.0 dB.** Lo slider volume in UI non deve
   fermarsi a 0 dB — quello è solo "unity gain", non il massimo. Il massimo
   vero è **+12 dB**. Se oggi lo slider arriva solo a 0, è un limite messo
   nell'app, va tolto.
2. **`PUT /api/audio/profile` sostituisce l'intero oggetto**, non solo il
   volume. Ogni volta che l'utente muove lo slider del volume, il body della
   PUT deve contenere ANCHE `active_source`, `eq` (tutte e 6 le bande) ed
   `enhancements`, con i valori correnti — non solo `{"master": {...}}`.
   Il modo corretto:
   - tieni sempre in memoria (o rileggi con GET) lo stato completo del
     profilo;
   - quando l'utente cambia il volume, aggiorna SOLO il campo `master` in
     quello stato locale;
   - invia l'intero oggetto aggiornato con PUT.
3. Normalmente `left_db` e `right_db` vanno impostati **uguali** con un unico
   slider "Volume" (non serve un secondo controllo per il bilanciamento L/R,
   a meno che non venga chiesto esplicitamente).
4. Valori fuori range (-96/+12) vengono rifiutati dal firmware con errore —
   clampa lato client prima di inviare.

Esempio completo di richiesta corretta (cambio solo il volume a -6 dB,
tutto il resto invariato):

```json
PUT /api/audio/profile
{
  "active_source": "radio",
  "master": {"left_db": -6, "right_db": -6},
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

---

## 2. VU-meter

**Endpoint nuovo, disponibile da oggi**:

```
GET /api/audio/levels
```

Risposta (tutti i valori in dBFS, tipicamente negativi, 0 = fondo scala):

```json
{
  "radio_in_left_db": -1.5,
  "radio_in_right_db": -1.5,
  "bluetooth_in_left_db": -0.9,
  "bluetooth_in_right_db": -1.0,
  "output_left_db": -1.9,
  "output_right_db": -2.0
}
```

Regole obbligatorie:

1. **Il firmware non fa polling né cache** — ogni chiamata GET rilegge live
   dal DSP in quel preciso istante. Se vuoi un meter che si aggiorna nel
   tempo, il polling periodico lo devi fare tu lato app.
2. **Frequenza consigliata: ogni 200-500 ms**, non più veloce — ogni
   chiamata impegna il bus I2C del dispositivo per 6 letture sequenziali
   (una per meter, il chip ha solo 2 registri hardware di cattura).
3. **Ferma il polling quando la schermata con i meter non è visibile**
   (es. `onDisappear` / quando l'utente cambia tab) — non lasciarlo attivo
   in background, non serve e spreca risorse sul dispositivo.
4. `radio_in_*` sono il livello Si4684 (post-compressore), `bluetooth_in_*`
   il livello ESP32, `output_*` il livello dopo Bass Boost (prima del
   limiter finale) — utile per capire dove mostrare quale barra.
5. Se la risposta HTTP non è 200 (es. 500), mostra i meter come "non
   disponibili" invece di un valore congelato/stantio.
