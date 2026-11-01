"""Ricava adau1452_selfboot.bin dalla scrittura EEPROM catturata da SigmaStudio+ su un finto target TCP.

Uso: extract_selfboot.py FRAMES OUT.bin [Program.bin DMem0.dat DMem1.dat]
FRAMES contiene una riga JSON per ogni transazione. Le pagine sono le scritture sulla cella 0x3C,
con l'indirizzo EEPROM in 0x36. Controlla la struttura secondo il datasheet Rev. D, pag. 52-53.
"""
import json, re, struct, sys

CELL_ADDR, CELL_DATA = 0x36, 0x3C


def cycles(path):
    cur = None
    out = []
    for f in map(json.loads, open(path)):
        if f['t'] != 'w':
            continue
        if f['a'] == 0x32:  # inizio della sequenza del programmatore
            cur = {}
            out.append(cur)
            addr = None
        elif cur is not None and f['a'] == CELL_ADDR:
            addr = int(f['d'], 16)
        elif cur is not None and f['a'] == CELL_DATA:
            cur[addr] = bytes.fromhex(f['d'])
    return [c for c in out if c]


def assemble(pages):
    size = max(pages) + len(pages[max(pages)])
    img = bytearray(b'\xff' * size)
    for a, d in pages.items():
        img[a:a + len(d)] = d
    assert sum(len(d) for d in pages.values()) == size, 'buchi fra le pagine'
    return bytes(img)


def parse(img):
    assert img[0] == 0xAA, 'sentinella'
    first = int.from_bytes(img[1:4], 'big')
    pll_div, fb, chk, mclk = img[5], img[7], img[9], img[11]
    print(f'header: primo blocco {first:#x}, PLL_DIV {pll_div}, PLL_FB_DIV {fb}, PLL_CHECKSUM {chk} '
          f'(atteso {fb + mclk + pll_div}), MCLK_OUT {mclk}, velocita {img[12:16].hex()}')
    assert chk == fb + mclk + pll_div, 'PLL_CHECKSUM'
    at, blocks = first, []
    while True:
        h = struct.unpack('>HHHH', img[at:at + 8])
        lst, mem, base, n, jump = h[0] >> 15, h[0] & 3, h[1], h[2], h[3]
        blocks.append((mem, base, img[at + 8:at + 8 + 4 * n]))
        print(f'blocco @{at:#06x}: LST {lst} MEM {mem} base {base:#06x} parole {n} jump {jump:#x}')
        at += 8 + 4 * n
        if lst:
            break
    foot = int.from_bytes(img[at:at + 8], 'big')
    words = sum(int.from_bytes(img[i:i + 4], 'big') for i in range(0, at, 4))
    print(f'footer @{at:#x}: {foot:#x}, somma a 32 bit calcolata {words:#x}, dimensione {len(img)}, coda {img[at + 8:].hex()[:40]}')
    assert foot == words, 'checksum'
    return blocks


def memory_words(path):
    return [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', open(path).read())]


def main():
    frames, out = sys.argv[1:3]
    cs = cycles(frames)
    imgs = [assemble(c) for c in cs]
    # La prima scrittura dopo l'avvio ha una velocita non inizializzata (0x9D2F35): valgono le successive, 0x0F = 9,2 MHz
    print(len(imgs), 'scritture catturate, dalla seconda in poi uguali:', all(i == imgs[-1] for i in imgs[1:]))
    assert len(imgs) >= 2 and all(i == imgs[-1] for i in imgs[1:])
    img = imgs[-1]
    blocks = parse(img)
    if len(sys.argv) > 3:
        prog = open(sys.argv[3], 'rb').read()
        for mem, base, data in blocks:
            if mem == 2:
                print('PM nel Program.bin:', prog.find(data[:64]) >= 0)
    open(out, 'wb').write(img)
    print('scritto', out, len(img))


main()
