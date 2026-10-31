"""Costruisce il programma DSP che lascia all'ESP32 l'accesso alla 25AA1024: MP0-2 uscite da celle DC, MP3 ingresso.
Stessi requisiti di build_digiradio_adau1452.py. Chiude il progetto aperto in SigmaStudio+."""
import os, re, glob
from thrift.transport import TSocket
from thrift.protocol import TBinaryProtocol
from PythonClient.SdkScripting.SdkScripting import Client
from PythonClient.SdkScripting.ttypes import Position

OUT = os.path.dirname(os.path.abspath(__file__)).replace(os.sep, '/') + '/eeprom_writer'
os.makedirs(OUT + '/export', exist_ok=True)
sock = TSocket.TSocket('127.0.0.1', 9090); sock.setTimeout(300000)
c = Client(TBinaryProtocol.TBinaryProtocol(sock)); sock.open()
CANVAS = 'ADAU145xSchematic_0'
IC = 'ADAU1452_0'

def must(r):
    if not r.IsSuccess: raise RuntimeError(r.Message)
    return r

def add(shape, label, x, y):
    r = must(c.AddShapeV2(canvasName=CANVAS, shapeName=shape, position=Position(x=float(x), y=float(y))))
    uid = [m for m in r.Message if m.startswith('Uid:')][0][4:]
    must(c.UpdateLabel(uid, label))
    return uid

def wire(a, ap, b, bp): must(c.AddConnectionV2(sourceUid=a, sourcePin=ap, targetUid=b, targetPin=bp))

must(c.CreateNewProject('DigiRadio_EepromWriter'))
must(c.AddShapeV2(canvasName='Diagram_0', shapeName='USBi', position=Position(x=200.0, y=100.0)))
must(c.AddShapeV2(canvasName='Diagram_0', shapeName='AnalogDevices.Plugins.Platforms.EVALADAU1452MINIZ_v1.0.0.0', position=Position(x=500.0, y=100.0)))
wire('PC_0', 'USB', 'USBi_0', 'USB')
wire('USBi_0', 'SPI', 'EVALADAU1452MINIZ_0', 'SPI')

# Nessuna porta audio serve qui
for k, v in {'POWER_ENABLE0_reg_SIN0_PWR': 'Disable serial input power', 'POWER_ENABLE0_reg_SIN1_PWR': 'Disable serial input power',
             'POWER_ENABLE0_reg_SIN2_PWR': 'Disable serial input power', 'POWER_ENABLE0_reg_SIN3_PWR': 'Disable serial input power',
             'POWER_ENABLE0_reg_SOUT0_PWR': 'Disable serial output power', 'POWER_ENABLE0_reg_SOUT1_PWR': 'Disable serial output power',
             'POWER_ENABLE0_reg_SOUT2_PWR': 'Disable serial output power', 'POWER_ENABLE0_reg_SOUT3_PWR': 'Disable serial output power'}.items():
    must(c.UpdateStringProperty(IC, k, v))

for n, label in enumerate(('Ss', 'Mosi', 'Sclk')):  # MP0 SS_M, MP1 MOSI_M, MP2 SCLK_M
    must(c.UpdateStringProperty(IC, f'MP{n}_MODE_reg_MP_ENABLE', f'Multipurpose function (MP{n})'))
    must(c.UpdateStringProperty(IC, f'MP{n}_MODE_reg_MP_MODE', 'Output without pull-up'))
    dc = add('AnalogDevices.Plugins.Algorithms.Sources.Sigma300.Sample.DCInput_v1.0.0.0', f'Cell_{label}', 0, n * 150)
    o = add('Sigma300.GeneralPurposeOutput', f'Pin_{label}', 300, n * 150)
    must(c.UpdateStringProperty(o, 'SelectedGPIOChannel', f'GPIO_{n}'))
    wire(dc, 'Output', o, 'Input')

must(c.UpdateStringProperty(IC, 'MP3_MODE_reg_MP_ENABLE', 'Multipurpose function (MP3)'))
must(c.UpdateStringProperty(IC, 'MP3_MODE_reg_MP_MODE', 'Input from pin'))
gi = add('Sigma300.GeneralPurposeInput', 'Pin_Miso', 0, 500)
must(c.UpdateStringProperty(gi, 'SelectedGPIOChannel', 'GPIO_3'))
rb = add('BasicAlgorithms.Sigma300.Sample.DSPReadBack', 'Cell_Miso', 300, 500)
wire(gi, 'Output', rb, 'Input')

must(c.SaveProject(OUT + '/DigiRadio_EepromWriter.ssprj'))
r = c.LinkCompileConnect(); print(r)
print('\n'.join(e[:260] for e in c.GetOutputViewEntries()[-6:]))
print(must(c.Export('DigiRadio_EepromWriter', OUT + '/export')).Message)
must(c.SaveProject(OUT + '/DigiRadio_EepromWriter.ssprj'))
for f in glob.glob(OUT + '/export/*PARAM.h'):
    s = open(f, encoding='utf8', errors='replace').read()
    open(f, 'w', encoding='utf8').write(re.sub(r'_CONVERT\((-?\d+),(\d+(?:E[-+]?\d+)?)\)', r'_CONVERT(\1.\2)', s))
sock.close()
