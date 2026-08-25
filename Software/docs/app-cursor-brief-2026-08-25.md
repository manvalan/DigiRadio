# Brief per Cursor — allineamento app iOS al firmware DigiRadioFinale

Questo documento è vincolante: ogni punto marcato **DEVE** è un requisito, non
un suggerimento. Se qualcosa non è chiaro o sembra in conflitto con codice
esistente, chiedi prima di improvvisare una soluzione diversa — non inventare
comportamenti non specificati qui.

Dispositivo di test reale: `http://192.168.1.62` (mDNS `digiradio-CC4DB4.local`).
Verifica ogni funzionalità contro il dispositivo vero prima di considerarla
finita, non solo con dati mock.

---

## 0. Problemi riportati oggi dall'utente (da risolvere tutti)

1. L'interfaccia non è "in stile Apple" — troppo grezza/disorganizzata.
2. I nuovi controlli DSP (selettore sorgente, Bass Boost, Stereo Spread) non
   sono stati implementati.
3. Non è possibile selezionare la sorgente di ingresso (radio/Bluetooth).
4. Il controllo del volume è sbagliato/inconsistente.
5. Le stazioni: uno scan completo produce una lista lunga sia FM che DAB, ma
   aprendo il tab FM o il tab DAB separatamente dice "nessuna stazione" —
   vanno unificate in un'unica lista.

Le sezioni seguenti danno il contratto dati esatto e i requisiti UI per
risolvere ciascuno di questi punti. Non è accettabile implementare solo una
parte e considerare il resto "per dopo" senza dirlo esplicitamente.

---

## 1. Selettore sorgente (sostituisce il vecchio mixer) — OBBLIGATORIO

Il firmware non ha più un mixer che combina sorgenti. **DEVE** esserci un
controllo a scelta singola (es. `Picker` segmented, non due slider separati)
con tre opzioni: **Radio**, **Bluetooth**, e opzionalmente **Test tone**
(quest'ultimo solo in una sezione diagnostica, non nella UI principale).

Contratto API — `PUT /api/audio/profile`, corpo completo (tutti i campi sono
obbligatori nella richiesta, il firmware non li fa opzionali):

```json
{
  "active_source": "radio",
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

`active_source` accetta **esattamente** le stringhe `"radio"`, `"bluetooth"`,
`"beep"` (minuscolo, invariato). Nessun altro valore.

`GET /api/audio/profile` restituisce lo stesso oggetto — usalo per
inizializzare il Picker all'avvio schermata, non assumere sempre "radio".

**Errore comune da evitare**: NON costruire il body facendo merge parziale di
un vecchio oggetto `"mixer"` — quel campo non esiste più, se lo mandi il
parser lo ignora e basta (non causa errore, ma non fa nulla).

---

## 2. Volume — contratto esatto

Il volume master è **nello stesso oggetto profilo**, campo `"master"`:

```json
"master": {"left_db": -6, "right_db": -6}
```

- Range valido: **-96.0 .. +12.0** (dB). Valori fuori range vengono rifiutati
  dal firmware (risposta di errore, non clamping silenzioso) — clampa lato
  client PRIMA di inviare.
- **Non fermare lo slider a 0 dB** — il firmware supporta fino a **+12 dB** di
  guadagno reale sul master. Se lo slider attuale si ferma a 0 e "sembra
  basso", è un limite arbitrario della UI, non del firmware: estendi il range
  fino a +12 dB.
- Normalmente L e R vanno impostati **uguali** con un unico slider "Volume"
  (non serve un controllo stereo separato per il volume master, a meno che
  non venga esplicitamente richiesto un bilanciamento L/R).
- Ogni `PUT /api/audio/profile` **sostituisce l'intero profilo** — se lo slider
  del volume invia solo `{"master": {...}}` senza gli altri campi
  (`active_source`, `eq`, `enhancements`), il firmware risponde con errore di
  campo mancante. **DEVI sempre inviare l'oggetto completo**, leggendo prima lo
  stato corrente con `GET /api/audio/profile` (o tenendolo in uno store locale
  sempre sincronizzato) e poi cambiando solo il campo che l'utente ha toccato.

Questo "invia sempre tutto l'oggetto" è quasi certamente la causa del
"volume sbagliato": se il codice attuale manda solo il volume da solo, il
firmware lo rifiuta o (se altri campi arrivano con default sbagliati) resetta
sorgente/EQ/enhancement senza che l'utente l'abbia chiesto.

---

## 3. Bass Boost e Stereo Spread — nuovi controlli, OBBLIGATORI

Due slider 0-100, indipendenti da sorgente/volume/EQ:

```
POST /api/audio/bass-enhance     {"level": 0-100}
POST /api/audio/stereo-enhance   {"level": 0-100}
```

- Questi sono endpoint **a sé stanti**, non fanno parte del body di
  `/api/audio/profile` per la scrittura (ma il loro stato corrente torna
  dentro `GET /api/audio/profile` → `"enhancements": {"bass_level":.., "stereo_level":..}`).
- Bass Boost è un algoritmo **dinamico**: su un tono fisso di test non si
  sente quasi nulla, è normale — non è un bug se durante un test con la
  radio spenta sembra "non fare niente".
- Non esiste più il vecchio comportamento per cui alzare questi livelli
  "bloccava" delle bande dell'equalizzatore manuale — l'EQ è ora sempre
  indipendente. Il campo `"locked"` per banda nell'array `eq[]` (in
  `GET /api/audio/profile`) è ora sempre `false` tranne la banda 0 (che è
  sempre `true`, invariato da prima — è un passa-alto fisso, non toccarlo).

---

## 4. Lista stazioni unificata — OBBLIGATORIO

Il backend ha **un solo elenco stazioni**, non due:

```
GET /api/stations
```

restituisce un array dove ogni stazione ha un campo `"band"` che vale
`"fm"` oppure `"dab"`, più i campi specifici di banda (`fm_frequency_khz` per
FM; `dab_freq_index`, `dab_service_id`, `dab_component_id` per DAB).

**Il bug riportato oggi** (lo scan produce una lista lunga, ma il tab FM o il
tab DAB dicono "nessuna stazione") indica che l'app sta usando due sorgenti
dati diverse — una per la lista generale/scan e una diversa (vuota o rotta)
per i tab FM/DAB filtrati. **DEVI unificare**: un solo `StationListStore` (o
equivalente) alimentato da `GET /api/stations`, con i tab FM e DAB che sono
semplicemente **filtri client-side** (`station.band == .fm` / `.dab`) sulla
STESSA lista, non due chiamate/store separati.

UI richiesta: un'unica lista in stile Apple (`List` con sezioni, o una vista
tipo Impostazioni), non due implementazioni diverse per FM e DAB — stesso
componente di riga, stesso stile, filtrato per banda.

---

## 4bis. RDS (nome stazione FM) — ora funziona davvero, da oggi

Fino a oggi il firmware non decodificava **mai** l'RDS (bug a tre livelli,
risolto). Ora `GET /api/tuner/status` e lo scan FM completo possono
restituire:

```json
"fm": {
  "frequency_khz": 92100,
  "rssi_dbuv": 57,
  "snr_db": 40,
  "station_name": "M DUE O",
  "radiotext": "...testo libero..."
}
```

`station_name` e `radiotext` sono **opzionali** — compaiono solo dopo qualche
secondo di ricezione stabile (l'RDS impiega tempo ad accumularsi), quindi
possono mancare subito dopo una sintonizzazione. Mostrali quando presenti
(es. nella card "Now Playing" e nella riga della lista stazioni FM),
altrimenti mostra la sola frequenza come già fai.

Se durante l'ascolto normale (non solo durante lo scan) il nome stazione o il
radiotext cambiano o compaiono per la prima volta, aggiorna la UI di
conseguenza — l'utente ha chiesto esplicitamente un piccolo banner/notifica
quando arriva un nuovo nome/messaggio RDS durante la riproduzione.

---

## 5. Stile UI — Apple, minimalista ma con una sezione grafica curata

- Componenti nativi SwiftUI: `Picker` segmented per la sorgente, `Slider` con
  `.tint()` per volume/bass/stereo, `List`/`Form` in stile Impostazioni per le
  stazioni e le opzioni tecniche. Niente controlli custom pesanti o griglie di
  bottoni non standard.
- Organizza per tab/sezione logica, non tutto in una schermata:
  - **Ascolto**: sorgente attiva, volume, stazione corrente.
  - **Suono**: EQ 6 bande, Bass Boost, Stereo Spread.
  - **Stazioni**: lista unificata FM+DAB (vedi §4), con scan.
  - **Bluetooth**: pairing, dispositivo connesso.
  - **Diagnostica**: tono di test, dettagli tecnici/versione firmware — non
    mescolare con i controlli quotidiani.
- Una sezione "grafica" curata è benvenuta (es. una card "Now Playing" con
  sfondo sfumato/blur, animazione leggera sul cambio sorgente). Ora **puoi**
  agganciarla a dati reali di livello audio — vedi §7, l'endpoint VU-meter è
  disponibile da oggi.

---

## 7. VU-meter — nuovo, disponibile da oggi

```
GET /api/audio/levels
```

Risposta:

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

Valori reali in dBFS, letti dal vivo dal DSP **ad ogni chiamata** — il
firmware non fa polling in background né cache. Se vuoi un meter che si
aggiorna in tempo reale, il polling lo fai tu lato app (es. ogni 200-500ms
mentre la schermata è visibile) — **fermalo quando la schermata non è a
video**, per non generare traffico I2C continuo inutile sul dispositivo.
Questa è la sezione "grafica" giusta per barre VU vere (radio in / BT in /
uscita), non un'animazione finta.

---

## 8. Checklist di autoverifica prima di considerare il lavoro finito

- [ ] Cambiare sorgente da Radio a Bluetooth nell'app cambia davvero l'audio
      sul dispositivo reale (non solo lo stato locale dell'app).
- [ ] Cambiare il volume aggiorna il volume reale e **non** resetta
      accidentalmente sorgente/EQ/enhancement.
- [ ] Bass Boost e Stereo Spread hanno uno slider visibile, funzionante, e il
      valore torna corretto dopo un refresh/riavvio app (persistente lato
      firmware).
- [ ] Il tab FM mostra le stazioni FM dello scan; il tab DAB mostra quelle
      DAB; entrambe vengono dalla stessa fonte dati.
- [ ] Nessuna schermata mostra contemporaneamente controlli tecnici
      (indirizzi DSP, tono di test) mescolati a quelli quotidiani.
- [ ] Lo slider del volume arriva fino a +12 dB, non si ferma a 0 dB.
- [ ] I VU-meter (se implementati) mostrano numeri che cambiano nel tempo con
      l'audio reale, e il polling si ferma quando la schermata non è visibile.
