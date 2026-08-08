# Audio bring-up — handoff per Claude Code

> **RISOLTO (2026-08-07):** audio confermato funzionante end-to-end su Bose
> (Si4684/ESP32 → ADAU1701 → BT1035 → A2DP). Causa finale del silenzio:
> la cella DSP "Beep - variable gain" (toolbox ADI Sound Generation) usata
> come tono di test richiede sia `ENABLE` **sia** `KICK` (il parametro
> trigger) per avviare davvero la generazione — scrivere solo `ENABLE`
> lasciava il generatore silenzioso nonostante tutto il resto della catena
> (clock I2S, `AT+I2SCFG=35` a 24-bit, `AT+A2DPAUDIO`, encoder SBC attivo)
> fosse già corretto. Vedi `Adau1701Driver::setBeepEnabled()`.
>
> Il tono di test 440 Hz ora è generato dall'oscillatore interno del DSP
> ADAU1701 (programma SigmaStudio), non più dall'ESP32. Il codice
> `esp32_i2s_tone` e `CONFIG_ESP32_I2S_TONE_TEST` descritti in questo
> documento sono stati rimossi dal firmware; il resto del documento resta
> come riferimento storico del bring-up.

**Data:** 2026-08-05  
**Contesto:** bring-up hardware DigiRadio su scheda reale (ESP32-S3 + Si4684 + ADAU1701 + FSC-BT1035 → Bose Solo II).  
**Problema utente:** *non si sente* nulla sul Bose nonostante connessione Bluetooth.  
**Stato:** fix A2DP streaming applicati e verificati parzialmente; test tono ESP32 ancora da confermare a orecchio; FM debole senza antenna.

---

## Setup laboratorio

| Parametro | Valore |
|-----------|--------|
| Serial | `/dev/cu.usbmodem1101` |
| WiFi STA | `MikiLab` |
| IP / mDNS | ~`192.168.1.56`, `digiradio-CC4DB4.local` |
| Speaker BT | Bose Solo II, MAC `BC87FAE69D6E` |
| Area FM test | Massa (MS) — RTL **100.9 MHz**, RAI1 **95.5**, Club FM **97.3** |
| Working dir build | `Software/` |

---

## Catena audio (SigmaStudio / hardware)

```
Si4684 I2S (slave)  → ADAU1701 MP0 (fader Si4674) ─┐
ESP32 I2S (slave)   → ADAU1701 MP1 (fader ESP32)  ─┤→ St Mixer1 → EQ → master → MP6 out
                                                     └→ BT1035 I2S slave (AUXCFG=3) → A2DP → Bose
```

- **ADAU1701** = I2S **master** @ 48 kHz  
- **ESP32 / BT1035 / Si4684** = I2S **slave**  
- GPIO I2S ESP32: `BCLK=6`, `LRCLK/WS=7`, `DOUT=16` → ADAU MP1  

---

## Modifiche effettuate (non committate)

Tutte le modifiche sono su branch `main`, **working tree dirty** (~48 file toccati in totale, molti pre-esistenti + sessione audio).

### 1. Bluetooth — A2DP streaming (causa principale del silenzio)

**Problema:** il BT1035 restava su `+A2DPSTAT=3` (Connected) senza passare a `4` (Streaming). Senza streaming il modulo non invia audio I2S→A2DP al Bose.

**Fix Feasycom §5.3.6:** dopo la connessione serve `AT+A2DPAUDIO=1`.

| File | Modifica |
|------|----------|
| `components/core/include/core/Bt1035At.hpp` | Doc + `buildBt1035A2dpAudioLine()` |
| `components/core/src/Bt1035At.cpp` | `AT+A2DPAUDIO=1/0` |
| `components/core/test/bt1035_at_test.cpp` | Test linee A2DPAUDIO |
| `components/drivers/bt1035/include/bt1035/Bt1035Driver.hpp` | `startA2dpAudio()`, `waitForA2dpStreaming()` |
| `components/drivers/bt1035/src/Bt1035Driver.cpp` | Poll A2DPSTAT; invia A2DPAUDIO=1 se bloccato su Connected; **non uscire subito** se query fallisce (retry fino a timeout) |
| `components/services/bluetooth/src/BluetoothService.cpp` | `ensureA2dpStreaming()` dopo connect/reconnect; boot reconnect attende STAT=4 |

**Verifica log (OK):**
```
I (Bt1035) A2DP audio start (AT+A2DPAUDIO=1)
I (Bt1035) stream wait: A2DPSTAT=4
I (BluetoothSvc) A2DP streaming OK
```

### 2. ADAU1701 — routing mixer boot

| File | Modifica |
|------|----------|
| `components/core/src/MixerState.cpp` | `radioFirst()` / `esp32First()` — selezione sorgente ST0/ST1 |
| `components/services/audio/src/AudioService.cpp` | `applyRadioFirstMix()` / `applyEsp32FirstMix()` |
| `main/hardware_bootstrap.cpp` | Dopo `loadAndApply()`: mix in base a Kconfig |

Con `CONFIG_ESP32_I2S_TONE_TEST=y`:
```
I (hw_boot) ADAU1701 ESP32 I2S input routed (esp32-first mix)
```

Senza tone test (FM):
```
I (hw_boot) ADAU1701 Si4684 input routed (radio-first mix)
```

### 3. Si4684 FM — I2S out + tune/scan

| File | Modifica |
|------|----------|
| `components/drivers/si4684/src/Si4684Driver.cpp` | `AUDIO_OUTPUT_CONFIG` bit I2SOUTEN @ boot; unmute; volume max; offset RSQ AN649; tune con fallback STC; band FM 87500–107900 kHz |
| `components/drivers/si4684/src/Si4684Tuner.cpp` | Volume default 63; `readStatus()` non sovrascrive freq con READFREQ stale |
| `components/services/tuner/src/TunerService.cpp` | Scan FM step +100 kHz; filtro SNR; `fmChipReadFrequency` in status |
| `components/core/include/core/TunerStatus.hpp` | Campo `fmChipReadFrequency` |

**Nota:** con filo antenna corto, `rssi=0` su 100.9 MHz è normale — non indica per forza bug software.

### 4. Test tono ESP32 → ADAU → BT (isolamento path)

| File | Modifica |
|------|----------|
| `main/Kconfig.projbuild` | `ESP32_I2S_TONE_TEST` (default y in Kconfig, disabilitare in produzione) |
| `main/esp32_i2s_tone.cpp` | Sinusoide 440 Hz, 48 kHz, sample 24-bit MSB in slot 32-bit, amp 0.5 |
| `main/esp32_i2s_tone.hpp` | API `start()` |
| `main/main.cpp` | Task `esp32_tone`: delay 15 s (lascia finire boot reconnect), poi `waitForA2DPStreaming`, poi I2S |
| `main/CMakeLists.txt` | Compila `esp32_i2s_tone.cpp` se Kconfig attivo |
| `sdkconfig.defaults` | `CONFIG_ESP32_I2S_TONE_TEST=y` |
| `sdkconfig` | `CONFIG_ESP32_I2S_TONE_TEST=y` (abilitato manualmente — prima era `# is not set` e il tone test **non veniva compilato**) |

### 5. FM probe boot (alternativa al tone test)

| File | Modifica |
|------|----------|
| `main/main.cpp` | `fm_probe` task stack **16384** (era 4096 → stack overflow); tune 100.9 MHz, volume 63 |

Attivo solo se `CONFIG_ESP32_I2S_TONE_TEST` è **disabilitato**.

---

## Build e flash

```bash
cd Software

# Tone test ON (bring-up attuale)
idf.py build
idf.py -p /dev/cu.usbmodem1101 flash monitor

# FM probe al posto del tone test
idf.py -DCONFIG_ESP32_I2S_TONE_TEST=n fullclean reconfigure build flash
```

**Attenzione:** `-DCONFIG_ESP32_I2S_TONE_TEST=y` da riga di comando **non basta** se `sdkconfig` esistente ha `# CONFIG_ESP32_I2S_TONE_TEST is not set`. Verificare:
```bash
rg CONFIG_ESP32_I2S_TONE_TEST build/config/sdkconfig.h
# deve mostrare: #define CONFIG_ESP32_I2S_TONE_TEST 1
```

Ultimo build riuscito: `digiradio.bin` ~`0x237fe0` bytes.

---

## Log osservati (sessione 2026-08-05 sera)

### A2DP — OK dopo fix

```
I (BluetoothSvc) reconnect saved speaker BC87FAE69D6E (BOSE SOLO 2)
I (Bt1035) A2DP audio start (AT+A2DPAUDIO=1)
I (Bt1035) stream wait: A2DPSTAT=4
I (BluetoothSvc) A2DP streaming OK
```

A volte compare brevemente `A2DPSTAT=5` prima del `4`.

### Tone test — problemi I2S (in corso)

**Primo flash (tone test non compilato):** partiva `fm_probe`, mix radio-first, nessun log `esp32_i2s`.

**Secondo flash (tone test compilato):**
1. Race: tone task partiva insieme al reconnect → `waitForA2dpStreaming` usciva subito (query fallita) → tono partiva **prima** dello streaming.
2. I2S init falliva:
   ```
   E (i2s_std) i2s_std_calculate_clock(68): sample rate is too large
   E (esp32_i2s) i2s_channel_init_std_mode failed
   ```
   Causa: `I2S_CLK_SRC_EXTERNAL` senza `ext_clk_freq_hz` coerente con `bclk_div`.

**Fix applicati (post [Fix no audio path](8719ea5e-ee61-48fe-ba57-7298b5638cf9)):**
- Rimosso `I2S_CLK_SRC_EXTERNAL`; slave usa BCLK/LRCLK da ADAU con `clk_src` default.
- `waitForA2dpStreaming`: continua a pollare se query fallisce (no early exit).
- Tone task: **delay 15 s** prima di attendere A2DP (evita race con boot reconnect).
- **Re-apply `applyEsp32FirstMix()`** + `startA2dpAudio()` subito prima del tono (NVS/integration sovrascriveva il mix al boot).
- I2S **24-bit** Philips + `mclk_multiple=384`; ampiezza tono 0.85.
- `sdkconfig`: `CONFIG_ESP32_I2S_TONE_TEST=y` obbligatorio (altrimenti tone test non compilato).

**Log verificati su serial (2026-08-05 ~20:15, I2S OK ma Bose silenzioso):**
```
I (Bt1035) stream wait: A2DPSTAT=4
I (BluetoothSvc) A2DP streaming OK
I (digiradio) A2DP streaming — starting I2S tone
I (digiradio) ADAU esp32-first mix re-applied before tone
I (esp32_i2s) I2S slave TX started (BCLK=6 WS=7 DOUT=16)
I (esp32_i2s) tone task: 440 Hz sine, 48000 Hz, ADAU I2S slave TX
I (esp32_i2s) i2s streaming 2048 bytes/frame to ADAU MP1
```

### FM (senza tone test)

```
I (digiradio) FM audio probe OK: 100900 kHz rssi=0 dBuV snr=0 dB locked=0
```
Segnale assente/ debole — serve antenna.

---

## Dove siamo arrivati

| Componente | Stato |
|------------|-------|
| Boot ADAU + profilo NVS | OK |
| BT1035 I2S slave (`AUXCFG=3`, `I2SCFG=67`) | OK |
| Reconnect Bose saved speaker | OK |
| **A2DP Streaming (STAT=4)** | **OK** — fix `AT+A2DPAUDIO=1` ([Fix no audio path](8719ea5e-ee61-48fe-ba57-7298b5638cf9)) |
| Mix ADAU esp32-first / radio-first | OK; **re-apply prima del tono** necessario (NVS) |
| **Test tono 440 Hz — firmware** | **OK** (I2S init + write verificati in log) |
| **Test tono 440 Hz — Bose** | **Silenzio** — probabile hardware ADAU MP6 → BT1035 o sorgente BT Bose |
| FM audio su Bose | Non testabile finché tono non si sente o RSSI > 0 |
| Utente | *«no nulla»* — silenzio persistente nonostante log software OK |

---

## Prossimi passi consigliati (priorità)

1. **Se log mostrano `A2DPSTAT=4` + `i2s streaming … bytes/frame` ma Bose muto** → debug **hardware**:
   - Oscilloscopio: BCLK/LRCLK GPIO 6/7; dati MP6 ADAU → BT1035; dati GPIO 16 → ADAU MP1.
   - Bose: sorgente BT = DigiRadio, telefono scollegato, volume alto.
   - Verificare saldature/cavi I2S tra ADAU e BT1035 (path dopo DSP).

2. **Se I2S init fallisce ancora**
   - Provare `I2S_DATA_BIT_WIDTH_24BIT` + slot 32-bit (allineamento ADAU MP1).
   - Provare `bclk_div` 4 o 12 (vedi test ESP-IDF `i2s_multi_dev` slave).
   - Verificare con oscilloscopio BCLK/LRCLK su GPIO 6/7 quando ADAU è bootato (ADAU deve essere master attivo prima di `i2s_channel_enable`).

3. **Se tono OK ma FM no**
   - Tornare a build senza tone test (`CONFIG_ESP32_I2S_TONE_TEST=n`).
   - Antenna FM; verificare `I2SOUTEN` Si4684 e `applyRadioFirstMix`.
   - Cercare `rssi>15`, `locked=1`.

4. **Se A2DPSTAT=4 ma silenzio totale**
   - Hardware: MP6 ADAU → pin I2S BT1035; alimentazione Bose.
   - Verificare master volume / mute ADAU (safeload profilo — non dovrebbe essere a −∞).

5. **Commit** — non richiesto dall'utente finora; quando pronto, separare almeno:
   - BT A2DPAUDIO / streaming
   - Si4684 I2SOUTEN + scan/tune
   - Tone test Kconfig (dev-only)

---

## File chiave (quick reference)

```
main/main.cpp                    — fm_probe vs esp32_tone task
main/hardware_bootstrap.cpp      — mix ADAU al boot
main/esp32_i2s_tone.cpp          — generatore tono I2S slave
main/Kconfig.projbuild           — ESP32_I2S_TONE_TEST
sdkconfig / sdkconfig.defaults   — Kconfig effettivo

components/drivers/bt1035/       — A2DPAUDIO, waitForA2dpStreaming
components/services/bluetooth/   — ensureA2dpStreaming al reconnect
components/services/audio/       — applyRadioFirstMix / applyEsp32FirstMix
components/core/MixerState.cpp   — preset mixer ST0/ST1
components/drivers/si4684/       — I2SOUTEN, tune, RSQ
components/services/tuner/       — FM scan step tune
```

---

## Note per Claude Code

- Leggere `AGENTS.md` e `.cursor/rules/` prima di modifiche.
- **Non committare** salvo richiesta esplicita utente.
- Il tone test è **solo bring-up** — disabilitare per produzione (`sdkconfig.defaults.production` o `CONFIG_ESP32_I2S_TONE_TEST=n`).
- Due task competono sulla UART BT1035 (reconnect + tone wait): il delay 15 s mitiga ma non elimina del tutto la contesa; alternativa migliore: avviare il tone **dal callback** post-`boot reconnect OK` in `BluetoothService`.
- Transcript conversazione Cursor: thread audio/FM, ID `d10654e5-d27a-4999-812e-f371dc48567a`.

---

## Comandi utili debug

```bash
# Monitor filtrato
idf.py -p /dev/cu.usbmodem1101 monitor | rg -i 'A2DP|esp32_i2s|hw_boot|tone|streaming|error'

# Verifica config tone test nel binario
rg CONFIG_ESP32_I2S_TONE_TEST build/config/sdkconfig.h

# Reset + cattura serial (python)
python3 -c "
import serial, time
s=serial.Serial('/dev/cu.usbmodem1101',115200,timeout=0.2)
s.dtr=False; s.rts=True; time.sleep(0.1); s.rts=False
t=time.time()
while time.time()-t<90:
    l=s.readline()
    if l: print(l.decode(errors='replace'), end='')
"
```
