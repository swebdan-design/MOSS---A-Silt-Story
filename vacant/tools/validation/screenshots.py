#!/usr/bin/env python3
"""Capture real screenshots of the running ROM (PyBoy, CGB mode).

  screenshots.py [outdir]    -> outdir/*.png (2x) and outdir/contact_sheet.png

Each district is visited from a crafted save (seed 1) placed beside a cell
that holds that district's acoustic landmark, with a locked weather mode.
These are emulator captures, not mock-ups.
"""
import subprocess
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/validation'))
from emu import Emu  # noqa: E402
import savefile  # noqa: E402

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/shots'
OUT.mkdir(parents=True, exist_ok=True)
DISTRICTS = ['streets', 'industrial', 'residential', 'lower', 'forgotten']
# weather_mode: 0 auto, 1 clear, 2 light rain, 3 heavy rain, 4 fog, 5 storm
WEATHER = {'streets': 2, 'industrial': 1, 'residential': 4, 'lower': 3, 'forgotten': 1}


def find(seed, d):
    r = subprocess.run([str(ROOT / 'build/host_world'), 'find', str(seed), str(d)],
                       capture_output=True, text=True, check=True)
    x, y, lm = (int(v) for v in r.stdout.split())
    return x, y


def boot_at(x, y, weather, seed=1):
    rec = savefile.build(world_seed=seed, px=x, py=y, weather_mode=weather)
    e = Emu(sram=savefile.sram_with(rec))
    e.tick(150)
    e.press('start', 8, 8)
    e.tick(20)
    e.press('a', 8, 8)
    e.tick(60 * 25)          # let weather levels settle
    return e


def save(img, name):
    img.resize((320, 288), Image.NEAREST).save(OUT / f'{name}.png')
    return img


def main():
    shots = {}
    e = Emu()
    e.tick(160)
    shots['title'] = save(e.pb.screen.image.convert('RGB'), 'title')
    e.stop()
    for d, name in enumerate(DISTRICTS):
        x, y = find(1, d)
        e = boot_at(x, y, WEATHER[name])
        e.hold('right', 16 * 3)
        e.tick(200)            # popup gone
        shots[name] = save(e.pb.screen.image.convert('RGB'), name)
        if name == 'lower':
            e.press('start')
            e.tick(20)
            shots['synth'] = save(e.pb.screen.image.convert('RGB'), 'synth')
        e.stop()
    # storm: wait for sheet lightning
    x, y = find(1, 0)
    e = boot_at(x, y, 5)
    best, bright = None, -1
    for _ in range(60 * 90):
        e.tick(1)
        if e.u8('pal_flash') > 8:
            img = e.pb.screen.image.convert('RGB')
            v = sum(img.convert('L').getdata())
            if v > bright:
                best, bright = img.copy(), v
            if bright > 0:
                break
    if best:
        shots['storm'] = save(best, 'storm')
    e.stop()
    # elevated railway with a passing train (atmosphere raised for frequent trains)
    r = subprocess.run([str(ROOT / 'build/host_world'), str(ROOT / 'build/emu/r.bin'), '1', '0', '0', '1', '1'],
                       capture_output=True, text=True, check=True)
    sx, sy = (int(v) for v in r.stdout.split(' at ')[1].split(','))
    for dy in range(0, 400):
        rr = subprocess.run([str(ROOT / 'build/host_world'), str(ROOT / 'build/emu/r.bin'), '1',
                             str(sx), str(sy + dy), '1', '1'], capture_output=True, check=True)
        if (ROOT / 'build/emu/r.bin').read_bytes()[0] in (VIA_T_OVER,):
            break
    params = [11, 10, 8, 6, 9, 0, 7, 4, 6, 5, 6, 10, 15, 10, 0, 0, 4, 1, 1]
    rec = savefile.build(world_seed=1, px=sx, py=sy + dy + 3, weather_mode=1, params=params)
    e = Emu(sram=savefile.sram_with(rec))
    e.tick(150)
    e.press('start', 8, 8)
    e.tick(20)
    e.press('a', 8, 8)
    for _ in range(60 * 120):
        e.tick(1)
        if e.u8('train_active'):
            e.tick(40)
            shots['train'] = save(e.pb.screen.image.convert('RGB'), 'train')
            break
    e.stop()
    names = [n for n in ['title', 'streets', 'industrial', 'residential', 'lower', 'forgotten',
                         'storm', 'train', 'synth'] if n in shots]
    cols = 3
    rows = (len(names) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * 328 + 8, rows * 296 + 8), (8, 7, 15))
    for i, n in enumerate(names):
        sheet.paste(shots[n].resize((320, 288), Image.NEAREST), (8 + (i % cols) * 328, 8 + (i // cols) * 296))
    sheet.save(OUT / 'contact_sheet.png')
    print('screenshots:', ', '.join(names))


def _via_id():
    import re
    h = (ROOT / 'build/gen/assets.h').read_text()
    return int(re.search(r'#define MT_VIA_T_OVER (\d+)u', h).group(1))


VIA_T_OVER = _via_id()

if __name__ == '__main__':
    main()
