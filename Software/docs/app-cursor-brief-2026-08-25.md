# Brief per Cursor — allineamento app iOS al nuovo firmware DSP (DigiRadioFinale)

Il firmware ADAU1701 è stato sostituito con un programma DSP molto più ricco
(224 parametri contro 74 prima). Questo documento descrive **cosa è cambiato
lato API HTTP** e **quali controlli nuovi puoi costruire in app**, più qualche
indicazione di UX. I dettagli implementativi Swift (struttura file, ViewModels
esistenti, tab bar) restano una tua scelta — qui do solo il contratto dati e
gli obiettivi funzionali.

Dispositivo di test: `http://192.168.1.62` (mDNS `digiradio-CC4DB4.local`).

---

## 1. Breaking change: `mixer` → `active_source`

**Prima**: `GET/PUT /api/audio/profile` aveva un oggetto `"mixer"` con guadagni
indipendenti per Si4684, ESP32 e i due leg del mixer — permetteva di
"mescolare" radio e Bluetooth insieme.

**Ora**: il firmware non ha più un mixer. C'è un **selettore di sorgente
esclusivo** — si ascolta una sorgente alla volta, come su un vero stereo.

```json
{
  "active_source": "radio",
  "master": {"left_db": 0, "right_db": 0},
  "eq": [ ... 6 bande, invariato ... ],
  "enhancements": {"stereo_level": 0, "bass_level": 0}
}
```

`active_source` accetta esattamente `"radio"`, `"bluetooth"`, `"beep"`.
`"beep"` è il tono di test interno (usato per diagnostica firmware, probabilmente
da nascondere in UI di produzione o mettere in una sezione "Diagnostica").

**Azione richiesta**: sostituire ogni UI che oggi mostra due slider indipendenti
(volume radio / volume BT) con un **selettore a scelta singola** (segmented
control o lista) tra Radio e Bluetooth. Non esiste più un modo per sentirli
mescolati.

---

## 2. `enhancements` ora pilota algoritmi DSP reali, non più un trucco EQ

**Prima**: `bass_level`/`stereo_level` sovrascrivevano silenziosamente alcune
bande dell'equalizzatore manuale (da cui il campo `"locked"` per banda, per
segnalarlo).

**Ora**: pilotano due blocchi DSP dedicati e indipendenti dall'EQ:
- `bass_level` (0-100) → **Bass Boost1**, un vero algoritmo ADI di "Dynamic
  Bass Boost" (filtro crossover + compander dinamico). Effetto udibile solo su
  contenuto reale con dinamica (radio, streaming) — su un tono fisso costante
  non si sente quasi nulla, è normale (l'algoritmo reagisce a variazioni di
  livello nel tempo).
- `stereo_level` (0-100) → **SPhat1** ("SuperPhat" Spatializer/stereo widener),
  un secondo algoritmo ADI dedicato.

Il campo `"locked"` per banda EQ **ora è sempre `false`** tranne la banda 0
(passa-alto fisso, sempre `true`, invariato da prima). Puoi quindi rimuovere
qualunque logica "banda grigia perché l'enhancement l'ha sovrascritta" —
l'EQ manuale ora è sempre indipendente dagli enhancement.

**Azione richiesta**: nessun cambio di forma dati per `enhancements` (stessi
due slider 0-100 di prima), ma puoi rimuovere la UI "banda bloccata" per le
bande 1-5 (resta solo per la banda 0, che era già così).

---

## 3. Non ancora disponibile lato firmware (in arrivo)

- **VU-meter / readback dei livelli** (in ingresso e in uscita) — il firmware
  ha 6 sensori di livello nel DSP ma il meccanismo di lettura (un registro
  indiretto dell'ADAU1701) non è ancora implementato. Non costruire ancora una
  UI che dipende da dati di livello in tempo reale dal firmware — se vuoi una
  sezione "grafica" ora, usa un placeholder o un'animazione generica non
  agganciata a dati reali, finché non arriva l'endpoint.
- **Voice Clarifier** — la cella DSP dedicata (`Gen Filter1`) esiste nella
  catena del segnale ma non è ancora tarata (passa tutto invariato). Nessuna
  API la pilota ancora. Non esporre ancora questo controllo in UI, o mettilo
  disabilitato/"prossimamente".
- **Mute in uscita** (pre-Output1/Output2) — non presente in questo export del
  firmware. Se serve, va aggiunto lato SigmaStudio prima di poter esporlo via API.

Ti avviso appena questi sono pronti lato firmware con l'endpoint esatto.

---

## 4. Riepilogo controlli disponibili ORA (tutti testati dal vivo sul dispositivo)

| Controllo | Endpoint | Corpo |
|---|---|---|
| Sorgente attiva | `PUT /api/audio/profile` (campo `active_source`) | `"radio"` \| `"bluetooth"` \| `"beep"` |
| Master volume | `PUT /api/audio/profile` (campo `master`) | `{"left_db":..,"right_db":..}`, range tipico -96..+12 dB |
| Equalizzatore (6 bande) | `PUT /api/audio/profile` (campo `eq`) | invariato: `gain_db`, `center_hz`, `q` per banda; banda 0 sempre inerte |
| Bass Boost | `POST /api/audio/bass-enhance` | `{"level":0-100}` |
| Stereo Spread | `POST /api/audio/stereo-enhance` | `{"level":0-100}` |
| Tono di test (diagnostica) | `POST /api/audio/beep` | `{"enabled":true/false}` — richiede anche `active_source:"beep"` per essere udibile |
| Lettura stato completo | `GET /api/audio/profile` | risposta con tutti i campi sopra |

---

## 5. Indicazioni di stile (dalla richiesta dell'utente)

- Stile Apple/HIG nativo: niente slider/bottoni "grezzi" o accozzaglia in
  un'unica schermata. Raggruppa per tab/sezione logica (es. "Ascolto" per
  sorgente+volume, "Suono" per EQ+Bass Boost+Stereo Spread, "Bluetooth" per
  pairing, "Diagnostica" per tono di test/dettagli tecnici).
- Preferisci componenti nativi SwiftUI (`Picker` segmented per la sorgente,
  `Slider` con `.tint()` per i livelli, liste con `Form`/`List` in stile
  Impostazioni) piuttosto che controlli custom pesanti.
- La sezione "grafica" (visualizzazione carina) può oggi mostrare solo dati
  già disponibili (EQ come curva, o un'animazione leggera legata allo stato
  sorgente/tono) — non agganciarla a VU-meter reali finché non sono pronti
  (punto 3).
- Dato che non esiste più il mix simultaneo radio+BT, il cambio sorgente è
  un'azione "netta" (come cambiare stazione) — vale la pena un feedback visivo
  chiaro (es. breve transizione/fade nell'interfaccia, non nell'audio: il
  cambio DSP è istantaneo) quando l'utente lo seleziona.
