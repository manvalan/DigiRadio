"""Costruisce DigiRadio-ADAU1452.ssprj via scripting SigmaStudio+ (porta 9090), senza download.
Richiede il client Python di Examples/ScriptingClient/Python/PythonClient in sys.path e `pip install thrift`."""
import sys, os, re
from thrift.transport import TSocket
from thrift.protocol import TBinaryProtocol
from PythonClient.SdkScripting.SdkScripting import Client
from PythonClient.SdkScripting.ttypes import Position

OUT = os.path.dirname(os.path.abspath(__file__)).replace(os.sep, '/')
PROJECT = OUT + '/DigiRadio_ADAU1452.ssprj'
sock = TSocket.TSocket('127.0.0.1', 9090); sock.setTimeout(300000)
c = Client(TBinaryProtocol.TBinaryProtocol(sock)); sock.open()
CANVAS = 'ADAU145xSchematic_0'

def must(r):
    if not r.IsSuccess: raise RuntimeError(r.Message)
    return r

def add(shape, label, x, y, canvas=CANVAS, **props):
    r = must(c.AddShapeV2(canvasName=canvas, shapeName=shape, position=Position(x=float(x), y=float(y))))
    uid = [m for m in r.Message if m.startswith('Uid:')][0][4:]
    must(c.UpdateLabel(uid, label))
    for k, v in props.items(): setp(uid, k, v)
    return uid

def setp(uid, k, v):
    if isinstance(v, bool): must(c.UpdateBooleanProperty(uid, k, v))
    # Con Windows in it-IT UpdateNumericProperty scambia il separatore (1.452 diventa 1452): si passa la stringa col punto
    elif isinstance(v, (int, float)): must(c.UpdatePropertiesV2(uid, k, repr(v)))
    elif isinstance(v, list): must(c.UpdateListProperty(uid, k, v))
    else: must(c.UpdateStringProperty(uid, k, v))

def wire(a, ap, b, bp): must(c.AddConnectionV2(sourceUid=a, sourcePin=ap, targetUid=b, targetPin=bp))

must(c.CreateNewProject('DigiRadio_ADAU1452'))
tc = add('AnalogDevices.Plugins.Communication.TCPIP_v1.0.0.0', 'TCPIP', 200, 100, canvas='Diagram_0')
must(c.UpdateStringProperty(tc, 'IPAddress', '127.0.0.1'))
# Il chip nudo non linka con USBi ("Unconnected input pins"): serve la piattaforma, che contiene un ADAU1452
must(c.AddShapeV2(canvasName='Diagram_0', shapeName='AnalogDevices.Plugins.Platforms.EVALADAU1452MINIZ_v1.0.0.0', position=Position(x=500.0, y=100.0)))
BOARD = 'EVALADAU1452MINIZ_0'
IC = 'ADAU1452_0'
wire('PC_0', 'USB', tc, 'USB')
wire(tc, 'SPI', BOARD, 'SPI')

# Registri (mappa pin: Hardware-v2/kicad/audio-dommain.kicad_sch)
regs = {
    # Radio: SDATA_IN0, il DSP genera i clock sui pin BCLK_IN0/LRCLK_IN0
    'SERIAL_BYTE_0_0_reg_BCLK_SRC': 'BCLK is master', 'SERIAL_BYTE_0_0_reg_LRCLK_SRC': 'LRCLK is master',
    # BT ed ESP32: i pin IN1/IN2 sono sullo stesso net di BCLK_OUT0/BCLK_OUT1, che li pilotano
    'SERIAL_BYTE_1_0_reg_BCLK_SRC': 'Slave from CLK domain 1', 'SERIAL_BYTE_1_0_reg_LRCLK_SRC': 'Slave from CLK domain 1',
    'SERIAL_BYTE_2_0_reg_BCLK_SRC': 'Slave from CLK domain 2', 'SERIAL_BYTE_2_0_reg_LRCLK_SRC': 'Slave from CLK domain 2',
    'SERIAL_BYTE_4_0_reg_BCLK_SRC': 'BCLK is master', 'SERIAL_BYTE_4_0_reg_LRCLK_SRC': 'LRCLK is master',
    'SERIAL_BYTE_5_0_reg_BCLK_SRC': 'BCLK is master', 'SERIAL_BYTE_5_0_reg_LRCLK_SRC': 'LRCLK is master',
    # Porte senza pin collegati
    'POWER_ENABLE0_reg_SIN3_PWR': 'Disable serial input power',
    'POWER_ENABLE0_reg_SOUT2_PWR': 'Disable serial output power',
    'POWER_ENABLE0_reg_SOUT3_PWR': 'Disable serial output power',
    'POWER_ENABLE1_reg_RX_PWR': 'Disable S/PDIF receiver power',
    'POWER_ENABLE1_reg_ADC_PWR': 'Disable ADC power',
    'POWER_ENABLE1_reg_PDM0_PWR': 'PDM power disable', 'POWER_ENABLE1_reg_PDM1_PWR': 'PDM power disable',
    # S/PDIF verso il TOSLINK J5
    'SPDIFTX_INPUT_reg_SPDIFTX_SOURCE': 'From DSP core program',
    'SPDIF_TX_EN_reg_TXEN': 'Enabled',
}
for k, v in regs.items(): setp(IC, k, v)

# Ingressi
inR = add('IO.Sigma300.Sample.Input0to15', 'Radio_In', 0, 0)
inB = add('IO.Sigma300.Sample.Input16to31', 'BT_In', 0, 250)
inE = add('IO.Sigma300.Sample.Input32to39', 'ESP32_In', 0, 500)
DC = 'Filters.Sigma300.Sample.DCBlocking'
sources = {}

def meter(name, src, pin, x, y):
    # Lettura dal firmware: registro MOD_METER_<nome>_VALUE_ADDR
    lv = add('AnalogDevices.Plugins.Algorithms.BasicAlgorithms.Sigma300.Sample.SingleLevelDetector_v1.0.0.0', f'Level_{name}', x, y + 40)
    rb = add('BasicAlgorithms.Sigma300.Sample.DSPReadBack', f'Meter_{name}', x + 150, y + 40)
    wire(src, pin, lv, 'Input0'); wire(lv, 'Output0', rb, 'Input')

for i, (name, uid, ch) in enumerate([('Radio', inR, (0, 1)), ('BT', inB, (16, 17)), ('ESP32', inE, (32, 33))]):
    sources[name] = []
    for j, side in enumerate('LR'):
        y = i * 250 + j * 100
        d = add(DC, f'DC_{name}_{side}', 200, y)
        wire(uid, f'Ch{ch[j]}_out', d, 'Input0')
        meter(f'{name}_{side}', d, 'Output0', 350, y)
        sources[name].append(d)

beep = add('Sources.Sigma300.Sample.BeepSource', 'Beep', 200, 750, Frequency=440)
split = add('MixersAndSplitters.Sigma300.Sample.SingleControlSplitter', 'Beep_Split', 400, 750, Gain=8)
wire(beep, 'Output', split, 'Input')

# Selettore: 0 Radio, 1 BT, 2 Beep (come il programma attuale), 3 ESP32
mux = add('MuxesAndDemuxes.Sigma300.Sample.StereoSwitchNx2', 'Source_Select', 600, 300, NumStereoChannels=4, SelectedVal=0)
for k, a in enumerate(sources['Radio'] + sources['BT']): wire(a, 'Output0', mux, f'Input{k}')
wire(split, 'Output0', mux, 'Input4'); wire(split, 'Output1', mux, 'Input5')
for k, a in enumerate(sources['ESP32']): wire(a, 'Output0', mux, f'Input{6 + k}')

# Compressore: stessa tabella di Compressor1 (ADAU1701), stesso rilevatore RMS stereo.
# ADAU1701: 34 punti da -90 dB a +9 dB, passo 3 dB. Sigma300: 53 punti da -153 dB a +3 dB, passo 3 dB.
# L'avviso "Immediate load value 9395240960" del compilatore compare con tutti i compressori RMS della libreria ADI.
V1_TABLE = [x / 2**23 for x in (
    0x80661B, 0x80661B, 0x835100, 0x864CDD, 0x895A16, 0x8C790E, 0x8FAA2F, 0x92EDE1, 0x964490, 0x99AEAA, 0x9D2CA2,
    0x9F8E54, 0x9DBECA, 0x9BF483, 0x9A2F6F, 0x986F7F, 0x96B4A5, 0x94FED2, 0x93F70D, 0x93DED2, 0x93C69B, 0x93AE68,
    0x939639, 0x937E0E, 0x9365E7, 0x7A851A, 0x61E8C1, 0x4E3DF6, 0x3E8685, 0x31F744, 0x27EDDE, 0x1FE897)]
COMP_TABLE = [V1_TABLE[0]] * 21 + V1_TABLE  # l'indice 21 del nuovo corrisponde a -90 dB
assert len(COMP_TABLE) == 53
comp = add('AnalogDevices.Plugins.Algorithms.DynamicsProcessors.Sigma300.Sample.NoExtDet2ChnlRMSNogain_v1.0.0.0', 'Compressor', 800, 300,
           TimeConstant=100, Hold=50, Decay=100)
must(c.UpdateListProperty(comp, 'Table', COMP_TABLE))
wire(mux, 'Output0', comp, 'Input0'); wire(mux, 'Output1', comp, 'Input1')
makeup = [add('Gain.Sigma300.Sample.Gain', f'Makeup_{s}', 1000, 250 + n * 150, Gain=1.25892541) for n, s in enumerate('LR')]
wire(comp, 'CompressedOutput0', makeup[0], 'Input'); wire(comp, 'CompressedOutput1', makeup[1], 'Input')

# EQ: i coefficienti esatti di Param EQ1 (ADAU1701). Il tipo 13 inverte il segno di A1/A2 quando compila
COEF = [
    (0.99815046787262, -1.99630105495453, 0.99815046787262, 1.99629759788513, -0.996304392814636),
    (0.99921977519989, -1.98560345172882, 0.986564874649048, 1.98560345172882, -0.985784649848938),
    (1.00035321712494, -1.95086574554443, 0.952795505523682, 1.95086574554443, -0.953148722648621),
    (0.995041608810425, -1.83803915977478, 0.86321759223938, 1.83803915977478, -0.858259201049805),
    (1.00251352787018, -1.5252513885498, 0.664113402366638, 1.5252513885498, -0.666626930236816),
    (1.01695561408997, -0.80655837059021, 0.41073751449585, 0.80655837059021, -0.427693128585815)]
eq = add('AnalogDevices.Plugins.Algorithms.Filters.Sigma300.Sample.GenSecondOrder_v1.0.0.0', 'EQ', 1200, 300, NumChannels=2, NumStages=6)
for s, (b0, b1, b2, a1, a2) in enumerate(COEF):
    setp(eq, f'Stage{s}_FilterType', 13)  # 13 = coefficienti diretti
    for k, v in zip(('B0', 'B1', 'B2', 'A1', 'A2'), (b0, b1, b2, -a1, -a2)): setp(eq, f'Stage{s}_{k}', v)
wire(makeup[0], 'Output', eq, 'Input0'); wire(makeup[1], 'Output', eq, 'Input1')

vol = [add('Gain.Sigma300.Sample.Gain', f'MasterVol_{s}', 1400, 250 + n * 150, Gain=0.501187) for n, s in enumerate('LR')]
wire(eq, 'Output0', vol[0], 'Input'); wire(eq, 'Output1', vol[1], 'Input')

sb = add('ADIAlgorithms.Sigma300.Sample.SuperBassStereo', 'SuperBass', 1600, 300, Intensity=1.3, XoverFreq=500)
wire(vol[0], 'Output', sb, 'Input0'); wire(vol[1], 'Output', sb, 'Input1')
phat = add('ADIAlgorithms.Sigma300.Sample.PhatStereo', 'StereoSpread', 1700, 300, SpreadLevel=-24.03)
wire(sb, 'Output0', phat, 'Input0'); wire(sb, 'Output1', phat, 'Input1')
out = {}
for n, s in enumerate('LR'):
    y = 250 + n * 150
    bb = add('ADIAlgorithms.Sigma300.Sample.DynamicBassBoostMono', f'BassBoost_{s}', 1800, y, BassFrequency=60, LowPassFrequency=100, LowThreshold=-25, HighThreshold=-18, CompressorRatio=1.11, Boost=5.92)
    lim = add('DynamicsProcessors.Sigma300.Sample.Limiter', f'Limiter_{s}', 2000, y, Threshold=-1.0, RMSTC=50, Decay=12)
    wire(phat, f'Output{n}', bb, 'Input0'); wire(bb, 'Output0', lim, 'Input')
    meter(f'Out_{s}', lim, 'Output', 2150, y)
    out[s] = lim

# Uscite: OUT0 ch0/1 BT1058, OUT1 ch16/17 ESP32, S/PDIF TOSLINK
for name, chs in [('BT', ('Ch0', 'Ch1')), ('ESP32', ('Ch16', 'Ch17'))]:
    for s, ch in zip('LR', chs):
        o = add('Sigma300.Output', f'{name}_Out_{s}', 2400, (0 if name == 'BT' else 200) + (0 if s == 'L' else 100), SelectedOPChannel=ch)
        wire(out[s], 'Output', o, 'Input')
for n, s in enumerate('LR'):
    o = add('Sigma300.SPDIFOutput', f'TOSLINK_{s}', 2400, 400 + n * 100, SelectedSPDIFOPChannel=f' {n}')
    wire(out[s], 'Output', o, 'Output')

must(c.SaveProject(PROJECT))
print('Salvato', PROJECT)
print('canvas', c.GetCanvasElements(CANVAS))
if '--compile' in sys.argv:
    print(c.LinkCompileConnect())
    print('\n'.join(c.GetOutputViewEntries()[-40:]))
    print(c.Export('DigiRadio_ADAU1452', OUT + '/export'))
    must(c.SaveProject(PROJECT))
    # Con Windows in it-IT l'header contiene CONVERT(0,5): lo riporta a C valido
    import glob
    for f in glob.glob(OUT + '/export/*PARAM.h'):
        s = open(f, encoding='utf8', errors='replace').read()
        s = re.sub(r'_CONVERT\((-?\d+),(\d+(?:E[-+]?\d+)?)\)', r'_CONVERT(\1.\2)', s)
        open(f, 'w', encoding='utf8').write(s)
sock.close()
