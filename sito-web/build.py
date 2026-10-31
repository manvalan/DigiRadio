#!/usr/bin/env python3
"""Genera il sito statico di DigiRadio (italiano in /, inglese in /en/).

Uso: python3 build.py [cartella_output]   (default: ../sito)
Le immagini stanno già in <output>/img/; questo script scrive HTML, style.css.
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / "sito"

GITHUB = "https://github.com/manvalan/DigiRadio"
RELEASES = GITHUB + "/releases"
ISSUES = GITHUB + "/issues"
HACKADAY = "https://hackaday.io/project/207209-digiradio"
HACKSTER = "https://www.hackster.io/mikbigi/digiradio-open-source-dab-fm-receiver-with-aptx-bluetooth-42d2b6"
HW_V1 = GITHUB + "/tree/main/Hardware"
HW_V2 = GITHUB + "/tree/main/Hardware-v2"
PCBWAY = ("https://www.pcbway.com/project/shareproject/"
          "DigiRadio_Open_Source_Hi_Fi_DAB_FM_Receiver_with_DSP_and_aptX_Adaptive_Blueto_a1952812.html")
PCBWAY_V2 = "https://www.pcbway.com/project/shareproject/DigiRadio_v2_44bdc08b.html"
PCBWAY_BADGE = (f'<a class="pcbway-badge" href="{PCBWAY_V2}"><img src="https://www.pcbway.com/project/img/images/frompcbway-1220.png" '
                'alt="PCB from PCBWay" /></a>')
FW_VERSION = "0.13.0"

PAGES = ["index", "v2", "hardware", "app", "firmware", "sviluppo", "privacy"]

UI = {
    "it": {
        "nav": {"index": "Home", "v2": "Novità v2", "hardware": "Hardware", "app": "App iOS", "firmware": "Firmware", "sviluppo": "Sviluppo", "privacy": "Privacy"},
        "other": "English", "other_code": "en",
        "footer": "DigiRadio è un progetto open source di Michele Bigi. Hardware CERN-OHL-S v2, firmware Apache 2.0.",
        "desc": "DigiRadio: ricevitore DAB+/FM open source con DSP e uscita Bluetooth aptX Adaptive, e la sua app per iPhone e iPad.",
    },
    "en": {
        "nav": {"index": "Home", "v2": "New: v2", "hardware": "Hardware", "app": "iOS app", "firmware": "Firmware", "sviluppo": "Development", "privacy": "Privacy"},
        "other": "Italiano", "other_code": "it",
        "footer": "DigiRadio is an open-source project by Michele Bigi. Hardware CERN-OHL-S v2, firmware Apache 2.0.",
        "desc": "DigiRadio: an open-source DAB+/FM receiver with a DSP and aptX Adaptive Bluetooth output, and its iPhone and iPad app.",
    },
}

TITLES = {
    "it": {"index": "DigiRadio", "v2": "DigiRadio v2", "hardware": "Hardware", "app": "App igiRadio", "firmware": "Firmware e aggiornamenti", "sviluppo": "Lo sviluppo", "privacy": "Privacy"},
    "en": {"index": "DigiRadio", "v2": "DigiRadio v2", "hardware": "Hardware", "app": "igiRadio app", "firmware": "Firmware and updates", "sviluppo": "Development story", "privacy": "Privacy"},
}

# ---------------------------------------------------------------- contenuti

def flow():
    return ('<div class="flow"><span class="chip">FM / DAB+</span><span class="arrow">→</span>'
            '<span class="chip">Si4684</span><span class="arrow">→ I²S →</span>'
            '<span class="chip">ADAU1701 DSP</span><span class="arrow">→ I²S →</span>'
            '<span class="chip">BT1035</span><span class="arrow">→</span><span class="chip">aptX</span></div>')


def links_cards(lang):
    it = lang == "it"
    return f"""
<div class="cards">
  <a class="card" href="{GITHUB}"><h3>GitHub</h3><p class="muted">{"Schema, Gerber, BOM e firmware." if it else "Schematic, Gerbers, BOM and firmware."}</p></a>
  <a class="card" href="{HACKADAY}"><h3>Hackaday.io</h3><p class="muted">{"La storia del progetto e i log." if it else "Project story and logs."}</p></a>
  <a class="card" href="{HACKSTER}"><h3>Hackster.io</h3><p class="muted">{"Il progetto con schema e componenti." if it else "The project with schematic and parts."}</p></a>
  <a class="card" href="{PCBWAY}"><h3>PCBWay</h3><p class="muted">{"Ordina la scheda già pronta." if it else "Order the board."}</p></a>
</div>"""


BODY = {}

BODY["it", "index"] = lambda: f"""
<section class="hero">
  <div>
    <h1>DigiRadio</h1>
    <p class="lead">Una radio DAB+ e FM open source che manda l'audio alle tue cuffie o alla tua cassa Bluetooth in aptX Adaptive, con un DSP nel mezzo e un'app per iPhone e iPad.</p>
    <div class="buttons">
      <a class="btn primary" href="v2.html">Scopri la v2</a>
      <a class="btn" href="hardware.html">Com'è fatta</a>
      <a class="btn" href="app.html">L'app igiRadio</a>
      <a class="btn" href="{GITHUB}">GitHub</a>
    </div>
  </div>
  <img src="img/scheda.jpg" width="1600" height="1200" alt="La scheda DigiRadio montata: ESP32-S3, sintonizzatore Si4684, DSP ADAU1701 e modulo Bluetooth Feasycom">
</section>

<a class="banner" href="v2.html">
  <img src="img/v2-render.jpg" width="974" height="1080" alt="Render 3D della scheda DigiRadio v2">
  <div>
    <span class="tag">Novità</span>
    <h2>DigiRadio v2</h2>
    <p>Va a batteria, ha un DSP ADAU1452 molto più potente, il Bluetooth FSC-BT1058 e un'uscita ottica S/PDIF. Tutto su una scheda a 6 strati da 60 × 80 mm.</p>
    <span class="more">Scopri la v2 →</span>
  </div>
</a>

<h2>Perché</h2>
<p>Volevo una radio che ricevesse DAB+ e FM e suonasse sulla mia cassa o sulle mie cuffie Bluetooth con una buona qualità, senza passare dal telefono. Quasi tutte le radio DAB+ hanno un loro altoparlante, e quelle con il Bluetooth di solito lo ricevono invece di trasmetterlo. Così l'ho progettata io.</p>

<h2>Come funziona</h2>
<p>Il percorso audio è tutto digitale, I²S a 48 kHz e 24 bit: nessuna conversione tra il sintonizzatore e il Bluetooth. L'unico DAC è quello delle cuffie o della cassa.</p>
{flow()}

<div class="cards">
  <div class="card"><h3>Radio</h3><p>DAB+, DAB e FM con RDS, testi scorrevoli, slideshow, frequenze alternative e scansione della banda.</p></div>
  <div class="card"><h3>Webradio e telefono</h3><p>Le webradio e l'audio del telefono via Wi-Fi passano dallo stesso DSP e arrivano in Bluetooth.</p></div>
  <div class="card"><h3>Suono</h3><p>Equalizzatore, preset audio, bassi e stereo allargato, calcolati dal SigmaDSP ADAU1701.</p></div>
  <div class="card"><h3>Sveglia</h3><p>Sveglie, timer di spegnimento e orologio sincronizzato via Internet.</p></div>
</div>

<h2>Tutto aperto</h2>
<p>Schema, PCB, distinta componenti e firmware sono pubblici: l'hardware con licenza CERN-OHL-S v2, il firmware con Apache 2.0. Su GitHub trovi la <a href="{HW_V1}">scheda v1</a> e la <a href="{HW_V2}">scheda v2</a>, che è anche <a href="{PCBWAY_V2}">su PCBWay</a>.</p>
{PCBWAY_BADGE}
{links_cards("it")}
"""

BODY["en", "index"] = lambda: f"""
<section class="hero">
  <div>
    <h1>DigiRadio</h1>
    <p class="lead">An open-source DAB+ and FM radio that sends its audio to your Bluetooth headphones or speaker in aptX Adaptive, with a DSP in the middle and an iPhone and iPad app.</p>
    <div class="buttons">
      <a class="btn primary" href="v2.html">Meet v2</a>
      <a class="btn" href="hardware.html">How it's built</a>
      <a class="btn" href="app.html">The igiRadio app</a>
      <a class="btn" href="{GITHUB}">GitHub</a>
    </div>
  </div>
  <img src="../img/scheda.jpg" width="1600" height="1200" alt="The assembled DigiRadio board: ESP32-S3, Si4684 tuner, ADAU1701 DSP and Feasycom Bluetooth module">
</section>

<a class="banner" href="v2.html">
  <img src="../img/v2-render.jpg" width="974" height="1080" alt="3D render of the DigiRadio v2 board">
  <div>
    <span class="tag">New</span>
    <h2>DigiRadio v2</h2>
    <p>It runs on a battery and has a much more powerful ADAU1452 DSP, the FSC-BT1058 Bluetooth module and an optical S/PDIF output. All on a 6-layer, 60 × 80 mm board.</p>
    <span class="more">Meet v2 →</span>
  </div>
</a>

<h2>Why</h2>
<p>I wanted a radio that receives DAB+ and FM and plays it on my own Bluetooth speaker or headphones with good quality, without going through a phone. Most DAB+ radios have their own small speaker, and the ones with Bluetooth usually receive it instead of transmitting it. So I designed my own.</p>

<h2>How it works</h2>
<p>The audio path is fully digital, I²S at 48 kHz and 24 bits: no conversions between the tuner and the Bluetooth link. The only DAC is the one in your headphones or speaker.</p>
{flow()}

<div class="cards">
  <div class="card"><h3>Radio</h3><p>DAB+, DAB and FM with RDS, scrolling text, slideshow, alternate frequencies and band scan.</p></div>
  <div class="card"><h3>Web radio and phone</h3><p>Web radio and your phone's audio over Wi-Fi go through the same DSP and out over Bluetooth.</p></div>
  <div class="card"><h3>Sound</h3><p>Equaliser, audio presets, bass and stereo enhancement, computed by the ADAU1701 SigmaDSP.</p></div>
  <div class="card"><h3>Alarm clock</h3><p>Alarms, sleep timer and a clock synced over the Internet.</p></div>
</div>

<h2>Fully open</h2>
<p>Schematic, PCB, bill of materials and firmware are public: hardware under CERN-OHL-S v2, firmware under Apache 2.0. On GitHub you'll find <a href="{HW_V1}">board v1</a> and <a href="{HW_V2}">board v2</a>, which is also <a href="{PCBWAY_V2}">on PCBWay</a>.</p>
{PCBWAY_BADGE}
{links_cards("en")}
"""



def flow_v2(lang):
    out = "uscita ottica" if lang == "it" else "optical out"
    return ('<div class="flow"><span class="chip">FM / DAB+</span><span class="arrow">→</span>'
            '<span class="chip">Si4684</span><span class="arrow">→ I²S →</span>'
            '<span class="chip">ADAU1452 DSP</span><span class="arrow">→ I²S →</span>'
            '<span class="chip">BT1058</span><span class="arrow">·</span>'
            f'<span class="chip">S/PDIF {out}</span></div>')


BODY["it", "v2"] = lambda: f"""
<section class="hero">
  <div>
    <p class="tag">Novità</p>
    <h1>DigiRadio v2</h1>
    <p class="lead">La seconda versione di DigiRadio va a batteria, ha un DSP molto più potente e un'uscita ottica S/PDIF. Tutto su una scheda a 6 strati da 60 × 80 mm.</p>
    <div class="buttons">
      <a class="btn primary" href="{PCBWAY_V2}">Il progetto su PCBWay</a>
      <a class="btn" href="{HW_V2}">File KiCad su GitHub</a>
    </div>
    {PCBWAY_BADGE}
  </div>
  <img src="img/v2-render.jpg" width="974" height="1080" alt="Render 3D della scheda DigiRadio v2">
</section>

<div class="note"><strong>A che punto è.</strong> Il progetto è completo e verificato: DRC con 0 errori, schema e PCB allineati, 0 bloccanti nella revisione di <a href="https://boardlab.it">BoardLab</a>. Il prossimo passo è produrre e montare il primo prototipo.</div>

<h2>Cosa cambia rispetto alla v1</h2>
<div class="table-wrap"><table>
  <tr><th></th><th>v1</th><th>v2</th></tr>
  <tr><td>DSP</td><td>ADAU1701</td><td><strong>ADAU1452</strong>: molta più potenza di calcolo, più porte I²S, ASRC, avvio da EEPROM</td></tr>
  <tr><td>Controller</td><td>ESP32-S3</td><td><strong>ESP32-S31-WROOM-3</strong> con 128 Mbit di flash esterna</td></tr>
  <tr><td>Bluetooth</td><td>FSC-BT1035</td><td><strong>FSC-BT1058</strong>, su una sua 3,3 V filtrata</td></tr>
  <tr><td>Uscite</td><td>Bluetooth</td><td>Bluetooth e <strong>ottica S/PDIF</strong> (TOSLINK)</td></tr>
  <tr><td>Alimentazione</td><td>USB-C</td><td>USB-C e <strong>batteria Li-ion</strong> con caricabatterie e misura della carica</td></tr>
  <tr><td>Scheda</td><td>6 strati, 50 × 90 mm</td><td>6 strati, <strong>60 × 80 mm</strong></td></tr>
</table></div>

<h2>Come funziona</h2>
<p>Il percorso audio resta tutto digitale. L'ADAU1452 è il centro: riceve tre ingressi I²S (radio, Bluetooth ed ESP32), rimanda l'audio al modulo Bluetooth e all'ESP32 e pilota l'uscita ottica S/PDIF, così la radio si collega anche a un amplificatore o a una soundbar.</p>
{flow_v2("it")}

<div class="cards">
  <div class="card"><h3>Si4684</h3><p class="muted">Silicon Labs</p><p>Sintonizzatore DAB/DAB+/FM con antenna su SMA, quarzo da 19,2 MHz e flash SPI per il suo firmware.</p></div>
  <div class="card"><h3>ADAU1452</h3><p class="muted">Analog Devices SigmaDSP</p><p>Il cuore audio: EQ, preset, mixer delle sorgenti e uscita S/PDIF. Si avvia da una EEPROM SPI da 1 Mbit.</p></div>
  <div class="card"><h3>FSC-BT1058</h3><p class="muted">Feasycom</p><p>Modulo Bluetooth audio comandato dall'ESP32 via UART.</p></div>
  <div class="card"><h3>ESP32-S31</h3><p class="muted">Espressif, modulo WROOM-3</p><p>Esegue il firmware, comanda radio e DSP, porta webradio e audio dal telefono nel DSP. Flash esterna da 128 Mbit e USB-C.</p></div>
</div>

<h2>A batteria</h2>
<p>La v2 è pensata per essere portatile. Un BQ25896 di Texas Instruments carica la batteria dalla USB-C e un MAX17048 misura quanta carica resta, così l'app potrà mostrarla.</p>
<ul>
  <li>Due buck-boost TPS63031 per le 3,3 V digitale e audio, separate.</li>
  <li>Un LDO TPS7A20 a basso rumore per la 1,8 V del sintonizzatore.</li>
  <li>Un buck TLV62568 per la 1,2 V del core del DSP.</li>
</ul>
<p class="muted">Per la batteria sto valutando due celle 18650 in parallelo oppure una batteria slim: la scelta non è ancora fatta.</p>

<h2>La scheda</h2>
<p>Sei strati a impedenza controllata, 50 Ω per la RF e 90 Ω differenziali per l'USB, in 60 × 80 mm. Tocca un'immagine per vederla a piena risoluzione.</p>
<div class="gallery two">
  <figure><a href="img/v2-render.jpg"><img src="img/v2-render.jpg" loading="lazy" alt="Render 3D della scheda v2"></a><figcaption>Render 3D: in alto l'ESP32-S31 e il sintonizzatore, al centro l'ADAU1452, in basso alimentazione e caricabatterie.</figcaption></figure>
  <figure><a href="img/v2-layout.jpg"><img src="img/v2-layout.jpg" loading="lazy" alt="Layout dello strato superiore della scheda v2"></a><figcaption>Il layout in KiCad, strato superiore.</figcaption></figure>
</div>

<h2>Verificata con BoardLab</h2>
<p>Prima di ordinarla ho fatto revisionare la scheda da <a href="https://boardlab.it">BoardLab</a>, un software di analisi dei PCB che sviluppo io. Legge schema, layout e il datasheet di ogni integrato e dice se la scheda si può ordinare, citando la pagina del datasheet da cui viene ogni osservazione. Sulle revisioni precedenti ha trovato problemi veri, come pull-up I²C mancanti, il carico e il drive level dei quarzi e il filtro del PLL del DSP. La storia completa è nella pagina <a href="sviluppo.html#boardlab">Sviluppo</a>.</p>

<h2>Produzione</h2>
<p>Il progetto è pubblicato su <a href="{PCBWAY_V2}">PCBWay</a>, con Gerber e distinta componenti, e partecipa al loro 9° Project Design Contest. Questa volta la scheda la monto io: da PCBWay ordino solo le PCB e i componenti li saldo a mano.</p>
{PCBWAY_BADGE}

<h2>Firmware e app</h2>
<p>L'obiettivo è usare lo stesso firmware della v1, adattato ai nuovi pin e al nuovo DSP, e la stessa app <a href="app.html">igiRadio</a> per entrambe le schede.</p>

<h2>Tutto aperto</h2>
<p>Progetto KiCad, schema e distinta componenti sono nella cartella <a href="{HW_V2}">Hardware-v2</a> su GitHub, con licenza CERN-OHL-S v2. Il <a href="download/digiradio-v2-step.zip">modello 3D STEP della scheda</a> (2,6 MB, zip) si può scaricare per progettare il contenitore.</p>
"""

BODY["en", "v2"] = lambda: f"""
<section class="hero">
  <div>
    <p class="tag">New</p>
    <h1>DigiRadio v2</h1>
    <p class="lead">The second version of DigiRadio runs on a battery and has a much more powerful DSP and an optical S/PDIF output. All on a 6-layer, 60 × 80 mm board.</p>
    <div class="buttons">
      <a class="btn primary" href="{PCBWAY_V2}">The project on PCBWay</a>
      <a class="btn" href="{HW_V2}">KiCad files on GitHub</a>
    </div>
    {PCBWAY_BADGE}
  </div>
  <img src="../img/v2-render.jpg" width="974" height="1080" alt="3D render of the DigiRadio v2 board">
</section>

<div class="note"><strong>Status.</strong> The design is complete and checked: 0 DRC errors, schematic and PCB in sync, 0 blockers in the <a href="https://boardlab.it">BoardLab</a> review. The next step is to fabricate and assemble the first prototype.</div>

<h2>What changes from v1</h2>
<div class="table-wrap"><table>
  <tr><th></th><th>v1</th><th>v2</th></tr>
  <tr><td>DSP</td><td>ADAU1701</td><td><strong>ADAU1452</strong>: far more processing power, more I²S ports, ASRCs, self-boot from EEPROM</td></tr>
  <tr><td>Controller</td><td>ESP32-S3</td><td><strong>ESP32-S31-WROOM-3</strong> with 128 Mbit of external flash</td></tr>
  <tr><td>Bluetooth</td><td>FSC-BT1035</td><td><strong>FSC-BT1058</strong>, on its own filtered 3.3 V rail</td></tr>
  <tr><td>Outputs</td><td>Bluetooth</td><td>Bluetooth and <strong>optical S/PDIF</strong> (TOSLINK)</td></tr>
  <tr><td>Power</td><td>USB-C</td><td>USB-C and a <strong>Li-ion battery</strong> with charger and fuel gauge</td></tr>
  <tr><td>Board</td><td>6 layers, 50 × 90 mm</td><td>6 layers, <strong>60 × 80 mm</strong></td></tr>
</table></div>

<h2>How it works</h2>
<p>The audio path stays fully digital. The ADAU1452 is the hub: it takes three I²S inputs (radio, Bluetooth and ESP32), sends audio back to the Bluetooth module and the ESP32, and drives the optical S/PDIF output, so the radio can also feed an amplifier or a soundbar.</p>
{flow_v2("en")}

<div class="cards">
  <div class="card"><h3>Si4684</h3><p class="muted">Silicon Labs</p><p>DAB/DAB+/FM tuner with an SMA antenna jack, its own 19.2 MHz crystal and SPI flash for its firmware.</p></div>
  <div class="card"><h3>ADAU1452</h3><p class="muted">Analog Devices SigmaDSP</p><p>The audio core: EQ, presets, source mixing and the S/PDIF output. It boots from a 1 Mbit SPI EEPROM.</p></div>
  <div class="card"><h3>FSC-BT1058</h3><p class="muted">Feasycom</p><p>Bluetooth audio module, controlled by the ESP32 over UART.</p></div>
  <div class="card"><h3>ESP32-S31</h3><p class="muted">Espressif, WROOM-3 module</p><p>Runs the firmware, controls the tuner and the DSP, and streams web radio and phone audio into the DSP. 128 Mbit external flash and USB-C.</p></div>
</div>

<h2>On battery</h2>
<p>v2 is designed to be portable. A Texas Instruments BQ25896 charges the battery from USB-C and a MAX17048 fuel gauge reports how much charge is left, so the app can show it.</p>
<ul>
  <li>Two TPS63031 buck-boost converters for separate digital and audio 3.3 V rails.</li>
  <li>A low-noise TPS7A20 LDO for the tuner's 1.8 V rail.</li>
  <li>A TLV62568 buck for the DSP's 1.2 V core.</li>
</ul>
<p class="muted">For the battery I'm weighing two 18650 cells in parallel or a slim pack: the choice isn't made yet.</p>

<h2>The board</h2>
<p>Six impedance-controlled layers, 50 Ω for RF and 90 Ω differential for USB, in 60 × 80 mm. Tap an image for full resolution.</p>
<div class="gallery two">
  <figure><a href="../img/v2-render.jpg"><img src="../img/v2-render.jpg" loading="lazy" alt="3D render of the v2 board"></a><figcaption>3D render: the ESP32-S31 and the tuner at the top, the ADAU1452 in the middle, power and charger at the bottom.</figcaption></figure>
  <figure><a href="../img/v2-layout.jpg"><img src="../img/v2-layout.jpg" loading="lazy" alt="Top layer layout of the v2 board"></a><figcaption>The KiCad layout, top layer.</figcaption></figure>
</div>

<h2>Checked with BoardLab</h2>
<p>Before ordering, I had the board reviewed by <a href="https://boardlab.it">BoardLab</a>, a PCB analysis tool I develop. It reads the schematic, the layout and the datasheet of every IC and tells you whether the board can be ordered, citing the datasheet page behind each finding. On earlier revisions it caught real issues, such as missing I²C pull-ups, crystal load and drive level, and the DSP PLL filter. The full story is on the <a href="sviluppo.html#boardlab">Development</a> page.</p>

<h2>Fabrication</h2>
<p>The project is published on <a href="{PCBWAY_V2}">PCBWay</a>, with Gerbers and bill of materials, and is entered in their 9th Project Design Contest. This time I'm assembling the board myself: I order only the bare PCBs from PCBWay and solder the parts by hand.</p>
{PCBWAY_BADGE}

<h2>Firmware and app</h2>
<p>The goal is to run the same firmware as v1, adapted to the new pins and the new DSP, and the same <a href="app.html">igiRadio</a> app on both boards.</p>

<h2>Fully open</h2>
<p>The KiCad project, schematic and bill of materials are in the <a href="{HW_V2}">Hardware-v2</a> folder on GitHub, under CERN-OHL-S v2. You can download the <a href="../download/digiradio-v2-step.zip">3D STEP model of the board</a> (2.6 MB, zip), for example to design an enclosure.</p>
"""


def gallery(prefix, caps):
    files = ["esp32-usb-power", "si4684-tuner", "bt1035-bluetooth", "adau1701-dsp"]
    items = "".join(
        f'<figure><a href="{prefix}img/{f}.jpg"><img src="{prefix}img/{f}.jpg" loading="lazy" alt="{c}"></a><figcaption>{c}</figcaption></figure>'
        for f, c in zip(files, caps))
    return f'<div class="gallery">{items}</div>'


BODY["it", "hardware"] = lambda: f"""
<h1>Hardware</h1>
<p class="lead">Una scheda a 6 strati da 50 × 90 mm con quattro chip principali, alimentata da USB-C.</p>

<div class="cards">
  <a class="card" href="{HW_V1}"><h3>Scheda v1 →</h3><p class="muted">Quella descritta in questa pagina: ADAU1701, BT1035. Schema, Gerber, BOM e file di montaggio.</p></a>
  <a class="card" href="v2.html"><h3>Scheda v2 →</h3><p class="muted">ADAU1452, batteria, BT1058, uscita ottica S/PDIF. Tutti i dettagli nella pagina della v2.</p></a>
</div>

<div class="cards">
  <div class="card"><h3>Si4684</h3><p class="muted">Silicon Labs</p><p>Sintonizzatore DAB+/DAB/FM, antenna esterna su connettore SMA, alimentazione dedicata a 1,8 V a basso rumore.</p></div>
  <div class="card"><h3>ADAU1701</h3><p class="muted">Analog Devices SigmaDSP</p><p>EQ, mixer delle sorgenti, bassi e stereo. È il master del clock I²S (quarzo 12,288 MHz) e tutto l'audio passa da lui.</p></div>
  <div class="card"><h3>FSC-BT1035</h3><p class="muted">Feasycom, Qualcomm QCC3056</p><p>Bluetooth 5.2 in uscita con aptX, aptX HD e aptX Adaptive.</p></div>
  <div class="card"><h3>ESP32-S3</h3><p class="muted">Espressif</p><p>Controller con Wi-Fi e BLE: avvia gli altri chip, porta le webradio nel DSP, espone l'API REST e gestisce gli aggiornamenti.</p></div>
</div>

<h2>Schema a blocchi</h2>
<pre>FM/DAB antenna → [ Si4684 ] --I²S--> [ ADAU1701 DSP ] --I²S--> [ FSC-BT1035 ] → aptX
                      ↑                     ↑                        ↑
                      └──── SPI ──── [ ESP32-S3 host ] ──── UART ────┘
                                            │  I²C
                                     Wi-Fi · NVS · HTTP API</pre>

<h2>La scheda</h2>
<ul>
  <li>6 strati a impedenza controllata, due piani di massa continui sotto RF, quarzi e USB.</li>
  <li>Tre zone d'antenna: le due a 2,4 GHz (ESP32 e BT1035) su angoli opposti, FM/DAB su SMA, con keep-out su tutti gli strati.</li>
  <li>USB-C, 3,3 V da un buck più un LDO a basso rumore a 1,8 V per il sintonizzatore.</li>
  <li>Il programma del DSP è fatto in SigmaStudio e si aggiorna via Wi-Fi senza riprogrammare l'ESP32.</li>
</ul>

<h2>Lo schema elettrico</h2>
<p class="muted">Tocca un'immagine per vederla a piena risoluzione. Il PDF e i file KiCad sono su <a href="{GITHUB}">GitHub</a>.</p>
{gallery("", ["ESP32-S3, USB-C e alimentazione", "Sintonizzatore Si4684", "Bluetooth FSC-BT1035", "DSP ADAU1701"])}

<h2>Produzione</h2>
<p>La scheda è stata prodotta e montata da <a href="https://www.pcbway.com/">PCBWay</a>, che ha sponsorizzato parte del costo. Con un sintonizzatore QFN, un DSP LQFP, due moduli a 2,4 GHz e un connettore USB-C su 6 strati, il montaggio a mano non era realistico. L'unico componente che non avevano è il modulo FSC-BT1035, che ho saldato io.</p>
<p>Puoi ordinarla dalla <a href="{PCBWAY}">pagina del progetto su PCBWay</a>. Trasparenza: PCBWay mi riconosce una piccola percentuale di quegli ordini come credito.</p>

<h2>Versione 2</h2>
<p>Tutti i dettagli sono nella <a href="v2.html">pagina della v2</a>. In breve: DSP ADAU1452 al posto dell'ADAU1701, caricabatterie BQ25896 per funzionare a batteria e modulo Bluetooth FSC-BT1058. Il layout è stato verificato con <a href="https://boardlab.it">BoardLab</a>, uno strumento di analisi dei PCB che sviluppo io: <a href="sviluppo.html#boardlab">cosa ha trovato</a>. I file sono nella cartella <a href="{HW_V2}">Hardware-v2</a> su GitHub.</p>
"""

BODY["en", "hardware"] = lambda: f"""
<h1>Hardware</h1>
<p class="lead">A 6-layer, 50 × 90 mm board with four main chips, powered over USB-C.</p>

<div class="cards">
  <a class="card" href="{HW_V1}"><h3>Board v1 →</h3><p class="muted">The one described on this page: ADAU1701, BT1035. Schematic, Gerbers, BOM and assembly files.</p></a>
  <a class="card" href="v2.html"><h3>Board v2 →</h3><p class="muted">ADAU1452, battery, BT1058, optical S/PDIF output. All the details on the v2 page.</p></a>
</div>

<div class="cards">
  <div class="card"><h3>Si4684</h3><p class="muted">Silicon Labs</p><p>DAB+/DAB/FM tuner, external antenna on an SMA connector, its own low-noise 1.8 V rail.</p></div>
  <div class="card"><h3>ADAU1701</h3><p class="muted">Analog Devices SigmaDSP</p><p>EQ, source mixing, bass and stereo enhancement. It is the I²S clock master (12.288 MHz crystal) and every audio source goes through it.</p></div>
  <div class="card"><h3>FSC-BT1035</h3><p class="muted">Feasycom, Qualcomm QCC3056</p><p>Bluetooth 5.2 output with aptX, aptX HD and aptX Adaptive.</p></div>
  <div class="card"><h3>ESP32-S3</h3><p class="muted">Espressif</p><p>Host controller with Wi-Fi and BLE: boots the other chips, streams web radio into the DSP, serves the REST API and handles updates.</p></div>
</div>

<h2>Block diagram</h2>
<pre>FM/DAB antenna → [ Si4684 ] --I²S--> [ ADAU1701 DSP ] --I²S--> [ FSC-BT1035 ] → aptX
                      ↑                     ↑                        ↑
                      └──── SPI ──── [ ESP32-S3 host ] ──── UART ────┘
                                            │  I²C
                                     Wi-Fi · NVS · HTTP API</pre>

<h2>The board</h2>
<ul>
  <li>6 layers, impedance-controlled, two solid ground planes under the RF, crystals and USB.</li>
  <li>Three antenna zones: the two 2.4 GHz antennas (ESP32 and BT1035) on opposite corners, FM/DAB on SMA, with keep-outs on every layer.</li>
  <li>USB-C, 3.3 V from a buck plus a low-noise 1.8 V LDO for the tuner.</li>
  <li>The DSP program is designed in SigmaStudio and can be updated over Wi-Fi without reflashing the ESP32.</li>
</ul>

<h2>Schematic</h2>
<p class="muted">Tap an image for full resolution. The PDF and KiCad files are on <a href="{GITHUB}">GitHub</a>.</p>
{gallery("../", ["ESP32-S3, USB-C and power", "Si4684 tuner", "FSC-BT1035 Bluetooth", "ADAU1701 DSP"])}

<h2>Fabrication</h2>
<p>The board was fabricated and assembled by <a href="https://www.pcbway.com/">PCBWay</a>, who sponsored part of the cost. With a QFN tuner, an LQFP DSP, two 2.4 GHz modules and a USB-C connector on 6 layers, hand assembly wasn't realistic. The only part they don't stock is the FSC-BT1035, which I soldered by hand.</p>
<p>You can order it from the <a href="{PCBWAY}">project page on PCBWay</a>. Disclosure: PCBWay gives me a small percentage of those orders as store credit.</p>

<h2>Version 2</h2>
<p>All the details are on the <a href="v2.html">v2 page</a>. In short: an ADAU1452 DSP instead of the ADAU1701, a BQ25896 charger for battery operation and an FSC-BT1058 Bluetooth module. The layout has been reviewed with <a href="https://boardlab.it">BoardLab</a>, a PCB analysis tool I develop: <a href="sviluppo.html#boardlab">what it found</a>. The files are in the <a href="{HW_V2}">Hardware-v2</a> folder on GitHub.</p>
"""

BODY["it", "app"] = lambda: f"""
<div class="split">
  <img class="phone" src="img/app-listen.jpg" width="600" height="1304" alt="La schermata Ascolta dell'app igiRadio">
  <div>
    <h1>igiRadio</h1>
    <p class="lead">L'app per iPhone e iPad che trova DigiRadio sulla rete di casa e la controlla tutta.</p>
    <div class="card" style="display:flex;gap:16px;align-items:center;margin:20px 0">
      <img src="img/app-icon.png" width="64" height="64" alt="" style="border-radius:14px">
      <div><strong>Beta su TestFlight</strong><br><span class="muted">Per provarla chiedi un invito aprendo una <a href="{ISSUES}">issue su GitHub</a>.</span></div>
    </div>

    <h2>Cosa fa</h2>
    <ul>
      <li><strong>Ascolta</strong>: accensione e standby, volume, sorgente FM, DAB o webradio, vu-meter e uscita Bluetooth.</li>
      <li><strong>Radio</strong>: sintonia, scansione FM e DAB, preset salvati sulla radio, RDS e testi DAB con copertine.</li>
      <li><strong>Suono</strong>: equalizzatore, preset audio, bassi e stereo, parametri del DSP.</li>
      <li><strong>Sveglie</strong> e timer di spegnimento.</li>
      <li><strong>Condividi</strong>: da Safari o da un'altra app mandi un link di webradio direttamente alla radio.</li>
      <li><strong>Siri e Comandi rapidi</strong>: accendi, metti in standby, sintonizza un preset, cambia volume, avvia la webradio, chiedi cosa sta suonando.</li>
      <li><strong>Configurazione</strong>: la prima volta l'app trova la radio via Bluetooth e le passa la rete Wi-Fi.</li>
      <li><strong>Aggiornamenti</strong>: controlla le nuove versioni del firmware e le installa via Wi-Fi.</li>
    </ul>

    <h2>Requisiti</h2>
    <p>iPhone o iPad e una DigiRadio sulla stessa rete Wi-Fi. L'app è in italiano e in inglese.</p>
    <p class="muted">Una versione per Android è prevista più avanti.</p>
  </div>
</div>
"""

BODY["en", "app"] = lambda: f"""
<div class="split">
  <img class="phone" src="../img/app-listen.jpg" width="600" height="1304" alt="The Listen screen of the igiRadio app">
  <div>
    <h1>igiRadio</h1>
    <p class="lead">The iPhone and iPad app that finds DigiRadio on your home network and controls all of it.</p>
    <div class="card" style="display:flex;gap:16px;align-items:center;margin:20px 0">
      <img src="../img/app-icon.png" width="64" height="64" alt="" style="border-radius:14px">
      <div><strong>Beta on TestFlight</strong><br><span class="muted">To try it, ask for an invite by opening a <a href="{ISSUES}">GitHub issue</a>.</span></div>
    </div>

    <h2>What it does</h2>
    <ul>
      <li><strong>Listen</strong>: power and standby, volume, FM, DAB or web radio source, VU meter and Bluetooth output.</li>
      <li><strong>Radio</strong>: tuning, FM and DAB scan, presets stored on the radio, RDS and DAB text with artwork.</li>
      <li><strong>Sound</strong>: equaliser, audio presets, bass and stereo, DSP parameters.</li>
      <li><strong>Alarms</strong> and sleep timer.</li>
      <li><strong>Share</strong>: send a web radio link from Safari or another app straight to the radio.</li>
      <li><strong>Siri and Shortcuts</strong>: turn on, standby, tune a preset, change volume, start web radio, ask what's playing.</li>
      <li><strong>Setup</strong>: the first time, the app finds the radio over Bluetooth and gives it your Wi-Fi network.</li>
      <li><strong>Updates</strong>: checks for new firmware and installs it over Wi-Fi.</li>
    </ul>

    <h2>Requirements</h2>
    <p>An iPhone or iPad and a DigiRadio on the same Wi-Fi network. The app is in English and Italian.</p>
    <p class="muted">An Android version is planned.</p>
  </div>
</div>
"""

BODY["it", "firmware"] = lambda: f"""
<h1>Firmware e aggiornamenti</h1>
<p class="lead">Il firmware gira sull'ESP32-S3 ed è scritto con ESP-IDF in C++23. Versione attuale: <strong>{FW_VERSION}</strong>.</p>

<h2>Come si aggiorna</h2>
<ol>
  <li>Apri igiRadio e vai in <em>Impostazioni</em> → <em>Firmware</em> → <em>Cerca aggiornamenti</em>.</li>
  <li>L'app scarica la nuova versione dalle <a href="{RELEASES}">release su GitHub</a> e la manda alla radio via Wi-Fi.</li>
  <li>La radio si riavvia con il nuovo firmware. Se entro 60 secondi qualcosa non va, torna da sola alla versione precedente.</li>
</ol>
<div class="note">Non serve un cavo né un computer, e le impostazioni e i preset restano sulla radio.</div>

<h2>Cosa c'è dentro</h2>
<div class="cards">
  <div class="card"><h3>Radio</h3><ul><li>FM con RDS e frequenze alternative</li><li>DAB+ con testi, DL+, slideshow</li><li>Scansione della banda</li></ul></div>
  <div class="card"><h3>Rete</h3><ul><li>Webradio, anche https</li><li>Audio dal telefono via Wi-Fi</li><li>API REST e scoperta mDNS</li></ul></div>
  <div class="card"><h3>Audio</h3><ul><li>EQ e preset audio salvati sulla radio</li><li>Cambio sorgente senza click</li><li>Bluetooth aptX Adaptive</li></ul></div>
  <div class="card"><h3>Sistema</h3><ul><li>Sveglie, sleep timer, orologio NTP</li><li>OTA a doppia partizione con rollback</li><li>Dati salvati cifrati</li></ul></div>
</div>

<h2>Storia delle versioni</h2>
<div class="table-wrap"><table>
  <tr><th>Versione</th><th>Novità</th></tr>
  <tr><td>0.13</td><td>DSP v2, preset audio salvati sulla radio, cambio sorgente senza click</td></tr>
  <tr><td>0.12</td><td>DAB DL+, scansione della banda, slideshow MOT, correzioni a memoria e stream</td></tr>
  <tr><td>0.11</td><td>Sveglie, sleep timer, orologio NTP, release automatiche</td></tr>
  <tr><td>0.10</td><td>Webradio, audio dal telefono, OTA con conferma e rollback</td></tr>
  <tr><td>0.9</td><td>FM con frequenze alternative, DAB, accensione e standby dall'app</td></tr>
</table></div>
<p>Il codice e tutte le release sono su <a href="{GITHUB}">GitHub</a>.</p>
"""

BODY["en", "firmware"] = lambda: f"""
<h1>Firmware and updates</h1>
<p class="lead">The firmware runs on the ESP32-S3 and is written with ESP-IDF in C++23. Current version: <strong>{FW_VERSION}</strong>.</p>

<h2>How to update</h2>
<ol>
  <li>Open igiRadio and go to <em>Settings</em> → <em>Firmware</em> → <em>Check for updates</em>.</li>
  <li>The app downloads the new version from the <a href="{RELEASES}">GitHub releases</a> and sends it to the radio over Wi-Fi.</li>
  <li>The radio restarts with the new firmware. If something goes wrong within 60 seconds, it rolls back to the previous version on its own.</li>
</ol>
<div class="note">No cable or computer needed, and your settings and presets stay on the radio.</div>

<h2>What's inside</h2>
<div class="cards">
  <div class="card"><h3>Radio</h3><ul><li>FM with RDS and alternate frequencies</li><li>DAB+ with text, DL+, slideshow</li><li>Band scan</li></ul></div>
  <div class="card"><h3>Network</h3><ul><li>Web radio, https too</li><li>Phone audio over Wi-Fi</li><li>REST API and mDNS discovery</li></ul></div>
  <div class="card"><h3>Audio</h3><ul><li>EQ and audio presets stored on the radio</li><li>Click-free source switching</li><li>aptX Adaptive Bluetooth</li></ul></div>
  <div class="card"><h3>System</h3><ul><li>Alarms, sleep timer, NTP clock</li><li>Dual-partition OTA with rollback</li><li>Encrypted storage</li></ul></div>
</div>

<h2>Version history</h2>
<div class="table-wrap"><table>
  <tr><th>Version</th><th>Highlights</th></tr>
  <tr><td>0.13</td><td>DSP v2, audio presets stored on the radio, click-free source switching</td></tr>
  <tr><td>0.12</td><td>DAB DL+, band scan, MOT slideshow, memory and stream fixes</td></tr>
  <tr><td>0.11</td><td>Alarms, sleep timer, NTP clock, automatic releases</td></tr>
  <tr><td>0.10</td><td>Web radio, phone audio, OTA with confirm and rollback</td></tr>
  <tr><td>0.9</td><td>FM alternate frequencies, DAB, power and standby from the app</td></tr>
</table></div>
<p>The code and all releases are on <a href="{GITHUB}">GitHub</a>.</p>
"""

BODY["it", "privacy"] = lambda: f"""
<h1>Privacy</h1>
<p class="lead">Informativa sulla privacy dell'app igiRadio. Ultimo aggiornamento: 29 ottobre 2026.</p>

<h2>In breve</h2>
<p>igiRadio non raccoglie dati personali, non ha account, pubblicità né strumenti di analisi. L'app parla soprattutto con la tua radio, sulla tua rete di casa.</p>

<h2>Cosa usa l'app e perché</h2>
<ul>
  <li><strong>Rete locale</strong>: per trovare la radio (mDNS) e controllarla.</li>
  <li><strong>Bluetooth</strong>: solo durante la prima configurazione, per trovare la radio e passarle la rete Wi-Fi. La password del Wi-Fi va solo alla radio.</li>
  <li><strong>Fotocamera</strong>: solo se scegli di leggere un codice QR di configurazione. Le immagini non vengono salvate né inviate.</li>
  <li><strong>Impostazioni</strong>: l'indirizzo della radio e le preferenze restano sul tuo dispositivo.</li>
</ul>

<h2>Servizi esterni</h2>
<ul>
  <li><strong>GitHub</strong>: quando cerchi aggiornamenti, l'app legge l'ultima release pubblica del firmware da github.com.</li>
  <li><strong>Apple iTunes Search</strong>: per mostrare la copertina, l'app può inviare ad Apple il titolo del brano o il nome della stazione che sta suonando.</li>
  <li><strong>Webradio</strong>: le stazioni che ascolti sono servite dai rispettivi server, che possono registrare le connessioni secondo le loro regole.</li>
</ul>
<p>Nessuno di questi servizi riceve dati che ti identificano dall'app.</p>

<h2>Contatti</h2>
<p>Per domande sulla privacy apri una <a href="{ISSUES}">issue su GitHub</a>.</p>
"""

BODY["en", "privacy"] = lambda: f"""
<h1>Privacy</h1>
<p class="lead">Privacy policy for the igiRadio app. Last updated: October 29, 2026.</p>

<h2>In short</h2>
<p>igiRadio does not collect personal data and has no accounts, ads or analytics. The app mostly talks to your radio, on your home network.</p>

<h2>What the app uses and why</h2>
<ul>
  <li><strong>Local network</strong>: to find the radio (mDNS) and control it.</li>
  <li><strong>Bluetooth</strong>: only during first setup, to find the radio and give it your Wi-Fi network. The Wi-Fi password goes only to the radio.</li>
  <li><strong>Camera</strong>: only if you choose to scan a setup QR code. Images are not saved or sent anywhere.</li>
  <li><strong>Settings</strong>: the radio's address and your preferences stay on your device.</li>
</ul>

<h2>External services</h2>
<ul>
  <li><strong>GitHub</strong>: when you check for updates, the app reads the latest public firmware release from github.com.</li>
  <li><strong>Apple iTunes Search</strong>: to show artwork, the app may send Apple the title of the song or the name of the station that is playing.</li>
  <li><strong>Web radio</strong>: the stations you listen to are served by their own servers, which may log connections under their own policies.</li>
</ul>
<p>None of these services receive data from the app that identifies you.</p>

<h2>Contact</h2>
<p>For privacy questions, open a <a href="{ISSUES}">GitHub issue</a>.</p>
"""

def report_table(lang):
    it = lang == "it"
    h = ("", "Report 4", "Report 6", "Report 7")
    rows = [
        ("Bloccanti" if it else "Blockers", "0", "0", "0"),
        ("Da rivedere" if it else "To review", "17", "22", "21"),
        ("Informativi" if it else "Info", "125", "106", "—"),
        ("Copertura" if it else "Coverage", "49 %", "62 %", "—"),
    ]
    head = "".join(f"<th>{c}</th>" for c in h)
    body = "".join("<tr>" + "".join(f"<td>{c}</td>" for c in r) + "</tr>" for r in rows)
    return f'<div class="table-wrap"><table><tr>{head}</tr>{body}</table></div>'


BODY["it", "sviluppo"] = lambda: f"""
<h1>Lo sviluppo di DigiRadio</h1>
<p class="lead">Come è nata la radio, dalla prima scheda al firmware 0.13, e come la seconda versione è stata revisionata con BoardLab prima di andare in produzione.</p>

<h2>Da dove sono partito</h2>
<p>Volevo una radio DAB+ e FM che mandasse l'audio alla mia cassa Bluetooth senza passare dal telefono e senza perdere qualità. Non l'ho trovata, quindi l'ho progettata.</p>
<p>L'idea di base non è mai cambiata: un percorso audio tutto digitale, I²S a 48 kHz e 24 bit, dal sintonizzatore al Bluetooth. In mezzo c'è un DSP che fa equalizzazione, mixer delle sorgenti e preset. A comandare tutto c'è un ESP32-S3, che si parla con un'app per iPhone.</p>
{flow()}

<h2>La scheda v1</h2>
<div class="split">
  <img src="img/scheda.jpg" width="1600" height="1200" loading="lazy" alt="La scheda DigiRadio v1 montata">
  <div>
    <p>La prima scheda è un 6 strati da 50 × 90 mm con quattro chip principali: il sintonizzatore Si4684 di Silicon Labs, il DSP ADAU1701 di Analog Devices, il modulo Bluetooth FSC-BT1035 con aptX Adaptive e l'ESP32-S3.</p>
    <p>Un sintonizzatore QFN, un DSP LQFP, due moduli a 2,4 GHz e un USB-C su 6 strati a impedenza controllata non si montano a mano. La scheda l'ha prodotta e montata PCBWay, che ha sponsorizzato parte del costo. L'unico componente che non avevano era il BT1035, e l'ho saldato io.</p>
  </div>
</div>
<p>Le prime settimane sono state di "messa in voce". Due esempi:</p>
<ul>
  <li><strong>Il formato I²S del Bluetooth.</strong> Il DSP manda frame da 32 bit per canale, e il BT1035 andava configurato per riceverli così. Un'impostazione sbagliata si sente subito come rumore o silenzio.</li>
  <li><strong>Il quarzo del Si4684.</strong> Il quarzo da 19,2 MHz girava qualche ppm sotto. Invece di cambiare componenti, il firmware lo calibra: misura lo scostamento sulle stazioni FM, corregge e salva il valore in EEPROM. Oggi l'errore residuo è di circa 3 ppm.</li>
</ul>

<h2>Il firmware, una versione alla volta</h2>
<p>Il firmware gira sull'ESP32-S3, è scritto con ESP-IDF in C++23 ed è cresciuto per versioni:</p>
<ul>
  <li><strong>0.9:</strong> FM con RDS e frequenze alternative, DAB, accensione e standby dall'app.</li>
  <li><strong>0.10:</strong> webradio, audio del telefono via Wi-Fi, aggiornamenti OTA. Se la nuova versione non conferma entro 60 secondi, la radio torna da sola a quella precedente.</li>
  <li><strong>0.11:</strong> sveglie, timer di spegnimento, orologio NTP. Da qui le release si pubblicano in automatico su GitHub e l'app le installa con "Cerca aggiornamenti".</li>
  <li><strong>0.12:</strong> DAB DL+, scansione della banda, slideshow MOT, correzioni a memoria e stream.</li>
  <li><strong>0.13:</strong> il nuovo programma del DSP, i preset audio salvati sulla radio e il cambio di sorgente senza click.</li>
</ul>
<div class="note">Una storia che racconta bene il lavoro: una mattina la radio si è riavviata esattamente all'ora della sveglia. La causa era un buffer da 4 KB creato sullo stack a ogni comando inviato al sintonizzatore. Dall'app funzionava, perché quel task ha uno stack grande, mentre il task della sveglia ne aveva solo 6 KB. Con un buffer da 64 byte e più stack per la sveglia il problema è sparito, e il giorno dopo la sveglia ha suonato senza riavvii.</div>

<h2>L'app igiRadio</h2>
<p>L'app per iPhone e iPad è il telecomando della radio: stazioni, preset, equalizzatore, sveglie, aggiornamenti. Parla con la radio via Wi-Fi attraverso un'API REST, e la trova da sola con mDNS. I preset audio stanno sulla radio, non sul telefono, quindi restano anche cambiando dispositivo.</p>
<p>Presto l'app sarà sull'App Store per iOS, e la versione per Android si potrà scaricare dal sito. Nel frattempo, chi vuole provarla può aprire una <a href="{ISSUES}">issue su GitHub</a> e riceverà la versione di test.</p>

<h2>Il suono: il DSP</h2>
<p>Il programma dell'ADAU1701 è fatto in SigmaStudio. L'ESP32 lo ricarica a ogni accensione, quindi si aggiorna via Wi-Fi senza riprogrammare niente.</p>
<p>Con la versione 2 del programma ho rivisto la catena: un compressore più leggero che non taglia più le parti piano, più margine prima dei bassi, sei preset di fabbrica (Neutro, Voce, Musica, Bassi, Classica, Notte) e fino a dieci preset personali. Le modifiche a SigmaStudio le abbiamo fatte anche via script, per avere un progetto ripetibile.</p>

<h2>La scheda v2</h2>
<p>La v1 funziona e la uso tutti i giorni, ma ha dei limiti chiari. Lo stream https delle webradio è al limite della memoria, il DSP è pieno e la radio va solo a corrente. La v2 nasce per risolverli:</p>
<ul>
  <li><strong>ADAU1452 al posto dell'ADAU1701.</strong> Molta più potenza di calcolo, più porte I²S, convertitori di frequenza (ASRC) e avvio autonomo da EEPROM.</li>
  <li><strong>ESP32-S3 con PSRAM</strong>, per webradio https stabili.</li>
  <li><strong>Caricabatterie BQ25896</strong>, per farla funzionare a batteria e renderla portatile. Per la batteria sto valutando due celle 18650 in parallelo oppure una batteria slim: la scelta non è ancora fatta.</li>
  <li><strong>Modulo Bluetooth FSC-BT1058</strong>, sempre con aptX Adaptive.</li>
  <li>Lo stesso stack-up a 6 strati: 50 Ω per la RF, 90 Ω differenziali per l'USB.</li>
</ul>
<p>L'ADAU1452 non è un cambio da un componente all'altro. Ha bisogno di un'alimentazione a 1,2 V che arrivi a quasi 700 mA, quindi ho aggiunto un buck dedicato. Ha un footprint LFCSP-72 con pad termico, un'altra gestione dei clock e un'altra mappa di pin. Ogni pin è stato verificato contro il datasheet.</p>
<p>Per non affidarmi solo alla memoria, ho confrontato la v2 con la scheda di valutazione di Analog Devices per l'ADAU1452. Da lì sono arrivate alcune correzioni: i condensatori del quarzo in NP0, un 1 µF in più sulla 1,2 V, una protezione ESD sull'antenna e il footprint giusto per la EEPROM.</p>

<h2 id="boardlab">BoardLab: la revisione prima dei Gerber</h2>
<p class="lead">Il layout della v2 l'ho fatto revisionare a <a href="https://boardlab.it">BoardLab</a>, un software di analisi dei PCB che sviluppo io.</p>

<h3>Cos'è</h3>
<p>Il DRC di KiCad controlla le regole geometriche: distanze, larghezze, collegamenti. Non sa niente di cosa fanno i chip. Una scheda può passare il DRC con zero errori e non accendersi.</p>
<p>BoardLab parte da lì. Legge lo schema e il PCB, legge i datasheet dei componenti e li confronta con il progetto, come farebbe un collega esperto in una revisione:</p>
<div class="cards">
  <div class="card"><h3>Collegamenti e alimentazioni</h3><p>Controlla che ogni chip sia alimentato, che i pin obbligatori siano collegati e che i domini di alimentazione non si alimentino l'uno con l'altro.</p></div>
  <div class="card"><h3>Regole dei datasheet</h3><p>Condensatori di disaccoppiamento vicino ai pin giusti, anelli di potenza corti, via di massa dove servono. Ogni segnalazione cita la pagina del datasheet.</p></div>
  <div class="card"><h3>Piste e correnti</h3><p>Verifica che le piste di potenza siano abbastanza larghe per la corrente che portano.</p></div>
  <div class="card"><h3>Antenne e disturbi</h3><p>Stima quanto i moduli a 2,4 GHz disturbano le piste vicine, scenario per scenario.</p></div>
  <div class="card"><h3>Piani di massa</h3><p>Segnala interruzioni dei piani sotto le coppie differenziali e sotto i nodi di commutazione.</p></div>
</div>
<p>Ogni punto è classificato come <strong>bloccante</strong>, <strong>da rivedere</strong> o <strong>informativo</strong>, con l'indicazione di quanta parte della scheda è stata coperta dall'analisi.</p>

<h3>Cosa ha trovato sulla v2</h3>
<p>Le prime revisioni sono state le più utili. Su uno schema che passava i controlli di KiCad, BoardLab ha trovato problemi che avrebbero reso la scheda inutilizzabile:</p>
<ul>
  <li><strong>Il modulo Bluetooth BT1058 non aveva alimentazione.</strong> La sua linea a 3,3 V esisteva, con i suoi condensatori, ma non era collegata a nessuna sorgente.</li>
  <li><strong>Il caricabatterie BQ25896 era isolato.</strong> Il suo ingresso non era collegato all'USB e la sua uscita non alimentava niente: la batteria non avrebbe mai alimentato la scheda.</li>
  <li><strong>L'interrupt del caricabatterie non arrivava all'ESP32.</strong></li>
</ul>
<p>Sono il tipo di errore che si scopre solo a scheda montata, con un nuovo ordine e qualche settimana persa.</p>
<p>Nelle revisioni successive i problemi sono diventati più fini:</p>
<ul>
  <li><strong>Pull-up su un dominio sbagliato.</strong> Due pull-up del bus SPI erano sulla 3,3 V digitale, sempre accesa, mentre i chip che servivano erano sulla 3,3 V audio, che si può spegnere. Con l'audio spento i chip sarebbero stati alimentati attraverso i diodi di protezione dei loro pin.</li>
  <li><strong>Un LED che non si sarebbe acceso.</strong> Un LED blu, che ha bisogno di circa 3,2 V, su 3,3 V con 1 kΩ in serie.</li>
  <li><strong>Piste di potenza da 0,127 mm</strong> negli anelli degli induttori dei regolatori, dove passa circa 1 A di picco.</li>
  <li><strong>Margine stretto sull'abilitazione del buck a 1,2 V</strong>: 1,34 V contro una soglia di 1,2 V. Con una resistenza diversa il margine è salito a 1,65 V.</li>
  <li><strong>La pista di reset del DSP vicino all'antenna dell'ESP32</strong>, e una zona di rame dimenticata su un segnale I²S.</li>
  <li><strong>Condensatori e via di massa</strong> da avvicinare ai pin, secondo i datasheet del caricabatterie, del modulo Bluetooth e del regolatore.</li>
</ul>

<h3>Da un report all'altro</h3>
{report_table("it")}
<p>Tra il report 4 e il report 6 i punti "da rivedere" sono saliti, ma non perché la scheda fosse peggiorata. Il report 6 controllava una parte in più e applicava le regole di layout dei datasheet del modulo Bluetooth, del caricabatterie e del regolatore. La copertura è passata dal 49 al 62 %.</p>
<p>Il report 7 ha chiuso tutti i consigli del report 6 e ne ha aggiunto uno nuovo: un condensatore da 1 µF sulla 1,2 V del DSP, come chiede il datasheet a pagina 193. È lo stesso punto che è emerso dal confronto con la scheda di valutazione.</p>
<div class="note">Alla fine la v2 è arrivata a <strong>0 bloccanti, DRC con 0 errori e schema e PCB allineati</strong>: pronta per i Gerber.</div>

<h3>Come lo uso</h3>
<p>BoardLab non decide al posto mio. Ogni segnalazione la verifico sul netlist e sul datasheet, e poi scelgo. Alcune le ho accettate così come sono: 18 via sotto il pad termico del DSP invece di 49 bastano per mezzo watt. Altre erano falsi positivi, per esempio una tabella pin del Si4684 estratta male.</p>
<p>La cosa che conta è che la revisione avviene prima di ordinare, quando correggere costa un clic e non una scheda nuova.</p>

<h2>Produzione e community</h2>
<p>La PCB v1 è stata prodotta e assemblata da <a href="https://www.pcbway.com/">PCBWay</a> (tranne il modulo BT), che ha sponsorizzato parte del costo, e si può ordinare dalla <a href="{PCBWAY}">pagina del progetto</a>: sempre gentili, cortesi e molto professionali. La v2 la monto io: da PCBWay ordino solo le PCB, i componenti li saldo da solo.</p>
<p>Il progetto è raccontato anche su <a href="{HACKADAY}">Hackaday.io</a>, su <a href="{HACKSTER}">Hackster.io</a> e su r/esp32.</p>

<h2>Tutto aperto</h2>
<p>Schema, PCB, distinta componenti e firmware sono su <a href="{GITHUB}">GitHub</a>: l'hardware con licenza CERN-OHL-S v2, il firmware con Apache 2.0.</p>

<h2>Cosa viene dopo</h2>
<ul>
  <li>Montare e collaudare la v2.</li>
  <li>Ricalibrare il quarzo del Si4684 sulla nuova scheda.</li>
  <li>Portare il firmware sull'ADAU1452 e sulla PSRAM.</li>
  <li>Scegliere la batteria: due 18650 in parallelo o una slim.</li>
  <li>L'app sull'App Store e la versione Android.</li>
</ul>
"""

BODY["en", "sviluppo"] = lambda: f"""
<h1>How DigiRadio was built</h1>
<p class="lead">How the radio came to be, from the first board to firmware 0.13, and how the second version was reviewed with BoardLab before going to fabrication.</p>

<h2>Where it started</h2>
<p>I wanted a DAB+ and FM radio that sent its audio to my Bluetooth speaker without going through a phone and without losing quality. I couldn't find one, so I designed it.</p>
<p>The core idea never changed: a fully digital audio path, I²S at 48 kHz and 24 bits, from the tuner to the Bluetooth link. In the middle there's a DSP for equalisation, source mixing and presets. An ESP32-S3 runs everything and talks to an iPhone app.</p>
{flow()}

<h2>Board v1</h2>
<div class="split">
  <img src="../img/scheda.jpg" width="1600" height="1200" loading="lazy" alt="The assembled DigiRadio v1 board">
  <div>
    <p>The first board is a 6-layer, 50 × 90 mm design with four main chips: the Silicon Labs Si4684 tuner, the Analog Devices ADAU1701 DSP, the FSC-BT1035 Bluetooth module with aptX Adaptive, and the ESP32-S3.</p>
    <p>A QFN tuner, an LQFP DSP, two 2.4 GHz modules and a USB-C connector on 6 impedance-controlled layers can't be assembled by hand. The board was fabricated and assembled by PCBWay, who sponsored part of the cost. The only part they didn't stock was the BT1035, which I soldered myself.</p>
  </div>
</div>
<p>The first weeks were spent bringing it up. Two examples:</p>
<ul>
  <li><strong>The Bluetooth I²S format.</strong> The DSP sends 32-bit frames per channel, and the BT1035 had to be configured to receive them that way. A wrong setting shows up straight away as noise or silence.</li>
  <li><strong>The Si4684 crystal.</strong> The 19.2 MHz crystal ran a few ppm low. Instead of changing parts, the firmware calibrates it: it measures the offset on FM stations, corrects it and stores the value in EEPROM. The residual error is now about 3 ppm.</li>
</ul>

<h2>The firmware, one version at a time</h2>
<p>The firmware runs on the ESP32-S3, is written with ESP-IDF in C++23, and grew version by version:</p>
<ul>
  <li><strong>0.9:</strong> FM with RDS and alternate frequencies, DAB, power and standby from the app.</li>
  <li><strong>0.10:</strong> web radio, phone audio over Wi-Fi, OTA updates. If a new version doesn't confirm itself within 60 seconds, the radio rolls back to the previous one on its own.</li>
  <li><strong>0.11:</strong> alarms, sleep timer, NTP clock. From here on, releases are published automatically on GitHub and the app installs them with "Check for updates".</li>
  <li><strong>0.12:</strong> DAB DL+, band scan, MOT slideshow, memory and stream fixes.</li>
  <li><strong>0.13:</strong> the new DSP program, audio presets stored on the radio and click-free source switching.</li>
</ul>
<div class="note">One story sums up the work well: one morning the radio rebooted exactly when the alarm went off. The cause was a 4 KB buffer created on the stack for every command sent to the tuner. It worked from the app, because that task has a large stack, but the alarm task only had 6 KB. With a 64-byte buffer and more stack for the alarm the problem went away, and the next day the alarm played without a reboot.</div>

<h2>The igiRadio app</h2>
<p>The iPhone and iPad app is the radio's remote: stations, presets, equaliser, alarms, updates. It talks to the radio over Wi-Fi through a REST API and finds it on its own with mDNS. Audio presets live on the radio, not on the phone, so they stay even if you switch devices.</p>
<p>The app will soon be on the App Store for iOS, and the Android version will be available to download from this site. In the meantime, if you'd like to try it, open an <a href="{ISSUES}">issue on GitHub</a> and you'll get the test version.</p>

<h2>The sound: the DSP</h2>
<p>The ADAU1701 program is designed in SigmaStudio. The ESP32 reloads it at every power-up, so it can be updated over Wi-Fi without reflashing anything.</p>
<p>With version 2 of the program I reworked the chain: a lighter compressor that no longer cuts the quiet passages, more headroom before the bass stage, six factory presets (Neutral, Voice, Music, Bass, Classical, Night) and up to ten user presets. Some of the SigmaStudio changes were made by script, so the project is reproducible.</p>

<h2>Board v2</h2>
<p>v1 works and I use it every day, but it has clear limits. https web radio streams are tight on memory, the DSP is full, and the radio only runs on mains power. v2 is designed to fix that:</p>
<ul>
  <li><strong>ADAU1452 instead of the ADAU1701.</strong> Much more processing power, more I²S ports, sample rate converters (ASRC) and self-boot from EEPROM.</li>
  <li><strong>ESP32-S3 with PSRAM</strong>, for stable https web radio.</li>
  <li><strong>BQ25896 charger</strong>, to run on battery and make it portable. For the battery I'm considering two 18650 cells in parallel or a slim pack: the choice isn't made yet.</li>
  <li><strong>FSC-BT1058 Bluetooth module</strong>, still with aptX Adaptive.</li>
  <li>The same 6-layer stack-up: 50 Ω for RF, 90 Ω differential for USB.</li>
</ul>
<p>The ADAU1452 is not a drop-in swap. It needs a 1.2 V rail that can reach almost 700 mA, so I added a dedicated buck converter. It comes in an LFCSP-72 package with a thermal pad, with different clocking and a different pin map. Every pin was checked against the datasheet.</p>
<p>So as not to rely on memory alone, I compared v2 with Analog Devices' evaluation board for the ADAU1452. That brought a few fixes: NP0 crystal capacitors, an extra 1 µF on the 1.2 V rail, ESD protection on the antenna and the right footprint for the EEPROM.</p>

<h2 id="boardlab">BoardLab: the review before the Gerbers</h2>
<p class="lead">I had the v2 layout reviewed by <a href="https://boardlab.it">BoardLab</a>, a PCB analysis tool I develop myself.</p>

<h3>What it is</h3>
<p>KiCad's DRC checks geometric rules: clearances, widths, connections. It knows nothing about what the chips do. A board can pass DRC with zero errors and still not power up.</p>
<p>BoardLab starts where DRC stops. It reads the schematic and the PCB, reads the component datasheets and checks the design against them, the way an experienced colleague would in a design review:</p>
<div class="cards">
  <div class="card"><h3>Connections and power</h3><p>Checks that every chip is powered, that mandatory pins are connected and that power domains don't back-feed each other.</p></div>
  <div class="card"><h3>Datasheet rules</h3><p>Decoupling capacitors next to the right pins, short power loops, ground vias where they're needed. Every finding cites the datasheet page.</p></div>
  <div class="card"><h3>Traces and currents</h3><p>Checks that power traces are wide enough for the current they carry.</p></div>
  <div class="card"><h3>Antennas and noise</h3><p>Estimates how much the 2.4 GHz modules couple into nearby traces, scenario by scenario.</p></div>
  <div class="card"><h3>Ground planes</h3><p>Flags plane splits under differential pairs and under switching nodes.</p></div>
</div>
<p>Each finding is classified as <strong>blocker</strong>, <strong>to review</strong> or <strong>info</strong>, together with how much of the board the analysis covered.</p>

<h3>What it found on v2</h3>
<p>The first reviews were the most valuable. On a schematic that passed KiCad's checks, BoardLab found problems that would have made the board useless:</p>
<ul>
  <li><strong>The BT1058 Bluetooth module had no power.</strong> Its 3.3 V rail existed, with its capacitors, but wasn't connected to any source.</li>
  <li><strong>The BQ25896 charger was isolated.</strong> Its input wasn't connected to USB and its output powered nothing: the battery would never have powered the board.</li>
  <li><strong>The charger's interrupt didn't reach the ESP32.</strong></li>
</ul>
<p>These are the kind of mistakes you only find once the board is assembled, with a new order and a few weeks lost.</p>
<p>In later reviews the findings became subtler:</p>
<ul>
  <li><strong>Pull-ups on the wrong domain.</strong> Two SPI pull-ups were on the always-on digital 3.3 V, while the chips they served were on the audio 3.3 V, which can be switched off. With audio off, those chips would have been powered through their pins' protection diodes.</li>
  <li><strong>An LED that would never light.</strong> A blue LED, which needs about 3.2 V, on 3.3 V with 1 kΩ in series.</li>
  <li><strong>0.127 mm power traces</strong> in the regulators' inductor loops, which carry about 1 A peak.</li>
  <li><strong>A thin margin on the 1.2 V buck enable</strong>: 1.34 V against a 1.2 V threshold. A different resistor raised it to 1.65 V.</li>
  <li><strong>The DSP reset trace next to the ESP32 antenna</strong>, and a forgotten copper zone on an I²S signal.</li>
  <li><strong>Capacitors and ground vias</strong> to move closer to the pins, per the charger, Bluetooth module and regulator datasheets.</li>
</ul>

<h3>From one report to the next</h3>
{report_table("en")}
<p>Between report 4 and report 6 the "to review" count went up, but not because the board got worse. Report 6 checked one more part and applied the layout rules from the Bluetooth module, charger and regulator datasheets. Coverage went from 49 to 62 %.</p>
<p>Report 7 closed every recommendation from report 6 and added a new one: a 1 µF capacitor on the DSP's 1.2 V rail, as the datasheet asks on page 193. It's the same point that came out of the evaluation board comparison.</p>
<div class="note">In the end v2 reached <strong>0 blockers, 0 DRC errors and a schematic and PCB in sync</strong>: ready for Gerbers.</div>

<h3>How I use it</h3>
<p>BoardLab doesn't decide for me. I check every finding against the netlist and the datasheet, then I choose. Some I accepted as they are: 18 vias under the DSP's thermal pad instead of 49 are enough for half a watt. Others were false positives, for example a badly extracted Si4684 pin table.</p>
<p>What matters is that the review happens before ordering, when a fix costs a click and not a new board.</p>

<h2>Fabrication and community</h2>
<p>The v1 PCB was fabricated and assembled by <a href="https://www.pcbway.com/">PCBWay</a> (except the Bluetooth module), who sponsored part of the cost, and you can order it from the <a href="{PCBWAY}">project page</a>: always kind, courteous and very professional. I'm assembling v2 myself: I order only the bare PCBs from PCBWay and solder the components on my own.</p>
<p>The project is also documented on <a href="{HACKADAY}">Hackaday.io</a>, on <a href="{HACKSTER}">Hackster.io</a> and on r/esp32.</p>

<h2>Fully open</h2>
<p>Schematic, PCB, bill of materials and firmware are on <a href="{GITHUB}">GitHub</a>: hardware under CERN-OHL-S v2, firmware under Apache 2.0.</p>

<h2>What's next</h2>
<ul>
  <li>Assemble and test v2.</li>
  <li>Recalibrate the Si4684 crystal on the new board.</li>
  <li>Port the firmware to the ADAU1452 and PSRAM.</li>
  <li>Choose the battery: two 18650s in parallel or a slim pack.</li>
  <li>The app on the App Store, and the Android version.</li>
</ul>
"""

# ---------------------------------------------------------------- template

def page(lang, name):
    ui = UI[lang]
    prefix = "" if lang == "it" else "../"
    other_prefix = "en/" if lang == "it" else "../"
    nav = "".join(
        f'<a href="{p}.html"{" aria-current=\"page\"" if p == name else ""}>{ui["nav"][p]}</a>'
        for p in PAGES)
    title = TITLES[lang][name]
    full_title = title if name == "index" else f"{title} · DigiRadio"
    return f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{full_title}</title>
<meta name="description" content="{ui["desc"]}">
<meta property="og:title" content="{full_title}">
<meta property="og:description" content="{ui["desc"]}">
<meta property="og:image" content="https://digiradio.michelebigi.it/img/scheda.jpg">
<link rel="alternate" hreflang="it" href="https://digiradio.michelebigi.it/{name}.html">
<link rel="alternate" hreflang="en" href="https://digiradio.michelebigi.it/en/{name}.html">
<link rel="icon" type="image/png" href="{prefix}favicon.png">
<link rel="apple-touch-icon" href="{prefix}img/app-icon.png">
<link rel="stylesheet" href="{prefix}style.css">
</head>
<body>
<header class="site-header"><div class="wrap">
  <a class="brand" href="index.html"><img src="{prefix}img/app-icon.png" alt="">DigiRadio</a>
  <nav class="nav">{nav}</nav>
  <a class="lang" href="{other_prefix}{name}.html" hreflang="{ui["other_code"]}">{ui["other"]}</a>
</div></header>
<main><div class="wrap">
{BODY[lang, name]().strip()}
</div></main>
<footer class="site-footer"><div class="wrap">
  <span>{ui["footer"]}</span>
  <span><a href="{GITHUB}">GitHub</a> · <a href="{HACKADAY}">Hackaday</a> · <a href="{HACKSTER}">Hackster</a></span>
</div></footer>
</body>
</html>
"""


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "en").mkdir(exist_ok=True)
    shutil.copy(HERE / "style.css", OUT / "style.css")
    for lang in ("it", "en"):
        for name in PAGES:
            dest = OUT / (f"{name}.html" if lang == "it" else f"en/{name}.html")
            dest.write_text(page(lang, name), encoding="utf-8")
    print("ok:", OUT)


if __name__ == "__main__":
    main()
