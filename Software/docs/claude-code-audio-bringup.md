# Claude Code — brief completo: audio bring-up DigiRadio

> **RISOLTO (2026-08-07):** audio confermato funzionante end-to-end su Bose.
> Causa finale: la cella DSP "Beep - variable gain" richiede sia `ENABLE`
> sia `KICK` per avviare la generazione — vedi
> `Adau1701Driver::setBeepEnabled()`. Il tono di test ora è generato
> dall'oscillatore interno del DSP ADAU1701, non più dall'ESP32; il codice
> `esp32_i2s_tone` e `CONFIG_ESP32_I2S_TONE_TEST` descritti in questo
> documento sono stati rimossi dal firmware. Resto del documento come
> riferimento storico del bring-up.

**Data:** 2026-08-05  
**Repo:** `/Users/michelebigi/Documents/Develop/DigiRadio`  
**Working dir firmware:** `Software/`  
**Priorità:** far sentire audio sul **Bose Solo II** (test tono 440 Hz, poi FM)

---

## Prompt iniziale (copia-incolla in Claude Code)

```
Stai lavorando su DigiRadio (ESP32-S3 firmware in Software/).

CONTESTO
- Bring-up audio su scheda reale: Si4684 → ADAU1701 → FSC-BT1035 → Bose Solo II.
- Serial: /dev/cu.usbmodem1101
- Speaker BT salvato: Bose Solo II MAC BC87FAE69D6E
- WiFi: MikiLab, mDNS digiradio-CC4DB4.local

PROBLEMA
L'utente non sente NULLA sul Bose. I log seriali mostrano però:
- A2DPSTAT=4 (streaming OK) dopo AT+A2DPAUDIO=1
- esp32_i2s: I2S slave TX started
- esp32_i2s: i2s streaming 2048 bytes/frame to ADAU MP1

Quindi il firmware BT + I2S sembra OK; il silenzio potrebbe essere:
1) routing/mute ADAU (NVS profile sovrascrive mix),
2) formato I2S ESP32↔ADAU (24 vs 32 bit),
3) cablaggio I2S ADAU MP6 → BT1035,
4) Bose (sorgente BT errata, telefono in competizione).

LEGGI PRIMA
- Software/AGENTS.md
- Software/docs/claude-code-audio-bringup.md (questo file)
- Software/docs/audio-bringup-handoff.md

OBIETTIVO
1. Far sentire un tono 440 Hz sul Bose (CONFIG_ESP32_I2S_TONE_TEST=y).
2. Poi FM su 100.9 MHz (Massa) con applyRadioFirstMix.

VINCOLI
- NON committare senza richiesta esplicita.
- Modifiche minime, seguire convenzioni esistenti.
- Build: idf.py build da Software/
- Flash: idf.py -p /dev/cu.usbmodem1101 flash monitor

TASK SUGGERITI (in ordine)
A) Verificare log post-boot (~30s): cercare mix re-applied, i2s streaming, errori write.
B) Se software OK ma muto: aggiungere test alternativo — safeload beep interno ADAU o DC test su master volume (se possibile via parametri esistenti).
C) Avviare tone task SOLO da BluetoothService dopo "boot reconnect OK" (elimina race UART).
D) Provare entrambi i formati I2S (24-bit Philips vs 32-bit MSB<<8) con Kconfig o retry.
E) Verificare che IntegrationService::startup() non muti il mixer durante tone test (skip applyProfile se tone test attivo).
F) Documentare pinout I2S ADAU↔BT1035 da docs/manual/ e Hardware/ per debug utente.

Quando finisci: riassumi cosa hai cambiato, comando flash, log attesi.
```

---

## Setup laboratorio

| Parametro | Valore |
|-----------|--------|
| MCU | ESP32-S3 (USB-JTAG `/dev/cu.usbmodem1101`) |
| Serial monitor | 115200 baud |
| WiFi STA | `MikiLab` |
| IP / mDNS | ~`192.168.1.56`, `digiradio-CC4DB4.local` |
| Unit serial | `D8478FCC4DB4` |
| Speaker BT | **Bose Solo II**, nome AT `BOSE SOLO 2`, MAC **`BC87FAE69D6E`** |
| Area FM (Massa MS) | RTL **100900** kHz, RAI1 **95500**, Club FM **97300** |
| Build | ESP-IDF 5.5, C++23, target `esp32s3` |

---

## Catena audio

```
Si4684 I2S (slave)  → ADAU1701 MP0 / Si4674 fader ─┐
ESP32 I2S (slave)   → ADAU1701 MP1 / ESP32 fader  ─┤→ St Mixer1 (ST0=Si4684, ST1=ESP32)
                                                     │   → Param EQ → master → MP6 SDATA_OUT0
                                                     └→ BT1035 I2S slave (AUXCFG=3, I2SCFG=67)
                                                         → A2DP encode → Bose Solo II
```

| Ruolo | Chip | Note |
|-------|------|------|
| I2S master 48 kHz | ADAU1701 | BCLK/LRCLK su MP10/MP11 |
| I2S slave TX test | ESP32 | BCLK=GPIO6, WS=GPIO7, DOUT=GPIO16 → ADAU MP1 |
| I2S slave RX | BT1035 | Riceve da ADAU MP6 |
| FM tuner | Si4684 | I2S out verso ADAU MP0; richiede `I2SOUTEN` |

Documentazione SigmaStudio: `Software/docs/manual/ch-adau1701.tex`, export in `Software/Firmware/ADAU1701-Firmware/`.

---

## Stato attuale (2026-08-05 sera)

| Layer | Stato | Evidenza log |
|-------|-------|--------------|
| ADAU boot + program load | OK | `hw_boot: companion chips ready` |
| BT1035 I2S slave | OK | `AT+AUXCFG=3, AT+I2SCFG=67` |
| Reconnect Bose | OK | `reconnect saved speaker BC87FAE69D6E` |
| A2DP Connected (3) | OK | `A2DPSTAT=3` |
| **A2DP Streaming (4)** | **OK** | fix `AT+A2DPAUDIO=1` |
| Mix esp32-first | OK (con re-apply) | `ADAU esp32-first mix re-applied before tone` |
| ESP32 I2S tone 440 Hz | **Firmware OK** | `I2S slave TX started`, `i2s streaming 2048 bytes/frame` |
| **Audio al Bose** | **FALLITO** | Utente: *«no nulla»* |
| FM 100.9 MHz | Segnale assente | `rssi=0` (antenna wire) |

**Conclusione:** il collo di bottiglia non è più A2DP Connected-vs-Streaming. Prossimo focus: path **ADAU MP6 → BT1035** oppure formato/mix, o Bose lato utente.

---

## Modifiche firmware (non committate)

### A. Bluetooth — A2DP streaming (FIX PRINCIPALE, verificato)

**Root cause:** BT1035 restava su `A2DPSTAT=3` (Connected). Serve `AT+A2DPAUDIO=1` (Feasycom §5.3.6) per `A2DPSTAT=4` (Streaming).

| File | Modifica |
|------|----------|
| `components/core/include/core/Bt1035At.hpp` | `buildBt1035A2dpAudioLine(bool establish)` |
| `components/core/src/Bt1035At.cpp` | `AT+A2DPAUDIO=1\r\n` / `=0` |
| `components/core/test/bt1035_at_test.cpp` | Unit test linee AT |
| `components/drivers/bt1035/include/bt1035/Bt1035Driver.hpp` | `startA2dpAudio()`, `waitForA2dpStreaming(int timeoutMs)` |
| `components/drivers/bt1035/src/Bt1035Driver.cpp` | Poll `A2DPSTAT`; retry `A2DPAUDIO=1` ogni 3 s su Connected; **no early return** se query fallisce |
| `components/services/bluetooth/src/BluetoothService.cpp` | `ensureA2dpStreaming()` dopo ogni connect/reconnect |

### B. ADAU1701 — mixer routing

| File | Modifica |
|------|----------|
| `components/core/include/core/MixerState.hpp` | `radioFirst()`, `esp32First()` |
| `components/core/src/MixerState.cpp` | ST0=Si4684, ST1=ESP32; mute path non usato a −96 dB |
| `components/services/audio/include/audio/AudioService.hpp` | `applyRadioFirstMix()`, `applyEsp32FirstMix()` |
| `components/services/audio/src/AudioService.cpp` | Safeload mixer + master unity |
| `main/hardware_bootstrap.cpp` | Dopo `loadAndApply()`: mix per Kconfig |

**Mapping SigmaStudio (St Mixer1):**
- `mixLeft` → ST0 (Si4684)
- `mixRight` → ST1 (ESP32)

**Bug scoperto:** `IntegrationService::startup()` → `recallPreset()` → `applyStoredAudioProfile()` può **sovrascrivere il mix** dopo `hw_boot`. Fix attuale: re-apply `applyEsp32FirstMix()` in `esp32I2sToneTask` prima di I2S. Fix migliore: skip apply profile durante tone test, o avviare tone da `BluetoothService` post-reconnect.

### C. Si4684 FM

| File | Modifica |
|------|----------|
| `components/drivers/si4684/src/Si4684Driver.cpp` | `AUDIO_OUTPUT_CONFIG` **I2SOUTEN** (0x0302=0x0002); unmute; vol 63; RSQ offsets AN649; tune STC fallback 150 ms |
| `components/drivers/si4684/src/Si4684Tuner.cpp` | Volume 63; READFREQ stale guard |
| `components/services/tuner/src/TunerService.cpp` | FM scan step +100 kHz; SNR≥10; chip freq check |
| `components/core/include/core/TunerStatus.hpp` | `fmChipReadFrequency` |

### D. Test tono ESP32 (bring-up dev)

| File | Modifica |
|------|----------|
| `main/Kconfig.projbuild` | `CONFIG_ESP32_I2S_TONE_TEST` (default y) |
| `main/esp32_i2s_tone.cpp` | 440 Hz sine, I2S slave, 24-bit Philips, mclk_multiple=384, amp 0.85 |
| `main/esp32_i2s_tone.hpp` | `bool start()` |
| `main/main.cpp` | Task `esp32_tone`: delay 15 s → wait A2DP → re-apply mix → `startA2dpAudio()` → I2S |
| `main/CMakeLists.txt` | Condizionale su Kconfig |
| `main/board_pins.hpp` | BCLK=6, WS=7, DOUT=16 |
| `sdkconfig.defaults` | `CONFIG_ESP32_I2S_TONE_TEST=y` |
| `sdkconfig` | `CONFIG_ESP32_I2S_TONE_TEST=y` (**critico** — se `# is not set` il tone non compila) |

### E. FM probe (alternativa, tone test OFF)

| File | Modifica |
|------|----------|
| `main/main.cpp` | `fm_probe` stack 16384; tune 100900 kHz; `applyRadioFirstMix` |

### F. Altri file dirty (sessioni precedenti, non audio-core)

Net/UI, secure store, BluetoothJson, SetupWebServer, station/tuner tests — vedi `git diff --name-only`.

---

## Boot sequence (ordine temporale)

```
app_main
  → HardwareBootstrap::boot()
       → ADAU program load
       → loadAndApply()          ← profilo NVS
       → applyEsp32FirstMix()    ← se tone test
       → BT1035 boot (AUXCFG=3)
       → Si4684 boot
  → integration.startup()       ← può recall preset + applyProfile (SOVRASCRIVE MIX)
  → NetBootstrap
  → bluetoothService.startupReconnect()  ← task async, A2DPSTAT→4
  → esp32I2sToneTask              ← delay 15s, wait stream, re-apply mix, I2S start
```

---

## Build, flash, verifica

```bash
cd /Users/michelebigi/Documents/Develop/DigiRadio/Software

# Verifica tone test compilato
rg CONFIG_ESP32_I2S_TONE_TEST build/config/sdkconfig.h
# Atteso: #define CONFIG_ESP32_I2S_TONE_TEST 1

idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor
```

**Disabilitare tone test (FM probe):**
```bash
# In sdkconfig: # CONFIG_ESP32_I2S_TONE_TEST is not set
idf.py fullclean reconfigure build flash
```

---

## Log attesi (tone test OK, ~30 s dopo reset)

```
I (hw_boot) ADAU1701 ESP32 I2S input routed (esp32-first mix)
I (Bt1035) I2S slave mode enabled (AT+AUXCFG=3, AT+I2SCFG=67)
I (BluetoothSvc) boot reconnect task started
I (BluetoothSvc) reconnect saved speaker BC87FAE69D6E (BOSE SOLO 2)
I (Bt1035) A2DP audio start (AT+A2DPAUDIO=1)
I (Bt1035) stream wait: A2DPSTAT=4
I (BluetoothSvc) A2DP streaming OK
I (digiradio) ==== ESP32 I2S tone test (440 Hz) — waiting A2DP streaming ====
I (digiradio) A2DP streaming — starting I2S tone
I (digiradio) ADAU esp32-first mix re-applied before tone
I (esp32_i2s) I2S slave TX started (BCLK=6 WS=7 DOUT=16)
I (esp32_i2s) tone task: 440 Hz sine, 48000 Hz, ADAU I2S slave TX
I (esp32_i2s) i2s streaming 2048 bytes/frame to ADAU MP1
```

**Errori già risolti (non devono riapparire):**
```
E (i2s_std) i2s_std_calculate_clock(68): sample rate is too large   ← I2S_CLK_SRC_EXTERNAL errato
W (digiradio) A2DP not streaming after 90000 ms  (a t=10s)           ← race waitForA2dpStreaming
```

---

## Ipotesi da investigare (Claude Code)

### H1 — Hardware I2S ADAU → BT1035
Firmware invia PCM a BT1035 solo se ADAU MP6 ha clock+dati. Verificare:
- MP6 configurato come SDATA_OUT0 (SigmaStudio `MFSELECT6=0x4` in export)
- `REG_SERIALOUTREGISTER1 = 0x800` (output enabled)
- Cavo dati MP6 → pin I2S DIN del BT1035

### H2 — Formato I2S ESP32 → ADAU MP1
ADAU accetta 24-bit I2S. Firmware usa 24-bit Philips; provare anche 32-bit con sample `<< 8` se tono ancora muto.

### H3 — NVS profile muta mixer/enhancements
`IntegrationService` richiama `applyProfile(currentProfile())` che riapplica mixer da NVS. Durante tone test:
- Disabilitare recall preset audio, oppure
- `#if CONFIG_ESP32_I2S_TONE_TEST` skip in `IntegrationService::applyStoredAudioProfile`

### H4 — Bose non in playout
- Telefono connesso al Bose in parallelo
- Bose su input AUX invece di BT
- Volume Bose al minimo

### H5 — BT1035 I2S non sincronizzato
Con `A2DPSTAT=4` e I2S da ADAU: verificare se BT1035 richiede comandi aggiuntivi oltre `A2DPAUDIO=1` (leggere `docs/manual/ch-bt1035.tex`, manuale Feasycom).

### H6 — Master volume / enhancements
`applyProfile` applica EQ con enhancements overlay. Verificare che master gain non sia −96 dB in profilo NVS salvato.

---

## Task concreti per Claude Code (checklist)

- [ ] **T1** Leggere log fresh dopo flash; confermare sequenza sopra.
- [ ] **T2** Spostare avvio tone in callback `BluetoothService` post `boot reconnect OK` (elimina delay 15s + race UART).
- [ ] **T3** Guard tone test: skip `IntegrationService::applyStoredAudioProfile` quando `CONFIG_ESP32_I2S_TONE_TEST`.
- [ ] **T4** Aggiungere Kconfig `ESP32_I2S_TONE_FORMAT_32BIT` per A/B test formato senza rebuild manuale.
- [ ] **T5** Log `queryA2dpState` + `A2DPENC` / volume BT1035 se esiste comando AT.
- [ ] **T6** Test senza ESP32: `applyRadioFirstMix` + Si4684 tone/noise (se RSSI>0 con antenna) per isolare BT path.
- [ ] **T7** Aggiornare `docs/audio-bringup-handoff.md` con esito.

---

## File chiave (aprire per primi)

```
Software/main/main.cpp
Software/main/esp32_i2s_tone.cpp
Software/main/hardware_bootstrap.cpp
Software/main/board_pins.hpp
Software/main/Kconfig.projbuild

Software/components/services/bluetooth/src/BluetoothService.cpp
Software/components/drivers/bt1035/src/Bt1035Driver.cpp
Software/components/core/src/Bt1035At.cpp

Software/components/services/audio/src/AudioService.cpp
Software/components/core/src/MixerState.cpp
Software/components/drivers/adau1701/src/Adau1701Driver.cpp

Software/components/services/integration/src/IntegrationService.cpp
Software/components/drivers/si4684/src/Si4684Driver.cpp

Software/Firmware/ADAU1701-Firmware/DigiRadio_IC_1_PARAM.h
Software/sdkconfig
Software/sdkconfig.defaults
```

---

## Regole progetto

- Leggere `Software/AGENTS.md` e `Software/.cursor/rules/` prima di ogni modifica.
- **Non committare** senza richiesta utente.
- Tone test = solo dev; produzione usa `sdkconfig.defaults.production` senza tone test.
- Host tests: `cmake --build build-host && ctest` (se tocchi core/bt1035).

---

## Comandi debug rapidi

```bash
# Monitor filtrato
idf.py -p /dev/cu.usbmodem1101 monitor 2>&1 | rg -i 'A2DP|esp32_i2s|hw_boot|tone|streaming|error|mix'

# Reset + 90s capture
python3 <<'PY'
import serial, time
s = serial.Serial("/dev/cu.usbmodem1101", 115200, timeout=0.2)
s.dtr = False; s.rts = True; time.sleep(0.1); s.rts = False
t = time.time()
while time.time() - t < 90:
    line = s.readline()
    if line: print(line.decode(errors="replace"), end="")
PY

# Host unit test BT1035 AT
cd Software && cmake -B build-host -DBUILD_HOST_TESTS=ON && cmake --build build-host && ctest --test-dir build-host -R bt1035
```

---

## Documenti correlati

| File | Contenuto |
|------|-----------|
| `Software/docs/claude-code-audio-bringup.md` | **Questo file** — brief operativo Claude Code |
| `Software/docs/audio-bringup-handoff.md` | Handoff tecnico sessione Cursor |
| `Software/docs/manual/ch-adau1701.tex` | ADAU boot, safeload, signal chain |
| `Software/docs/manual/ch-bt1035.tex` | BT1035 I2S, AUXCFG, pairing |
| `Software/AGENTS.md` | Regole agenti, build, DoD |

---

## Storia sessione Cursor

- Thread: audio/FM bring-up, ID `d10654e5-d27a-4999-812e-f371dc48567a`
- Subagent [Fix no audio path](8719ea5e-ee61-48fe-ba57-7298b5638cf9): fix `AT+A2DPAUDIO=1` + tone test scaffold
- Follow-up Cursor: sdkconfig tone test, fix I2S slave clock, mix re-apply, flash multipli
- Utente conferma silenzio persistente nonostante log software OK → sospetto hardware o mix/NVS
