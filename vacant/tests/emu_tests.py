#!/usr/bin/env python3
"""VACANT emulator tests: run the real ROM in PyBoy (Game Boy Color mode).

  1  boot and title screen (matches the title picture)
  2  new city: walker placed on a walkable metatile
  3  streaming: after long random walks, every visible hardware map entry
     (tile, VRAM bank, flips, palette) equals what the host generator says
  4  collision: the walker never stands on a solid metatile
  5  world seam: a save placed at x,y = 65533 walks across 65535 -> 0
  6  listening mode freezes the walker while the music keeps time
  7  synthesiser: parameter edits, music running inside the menu
  8  presets: each factory preset loads its values
  9  weather lock: heavy rain shows rain sprites and drives the music
 10  saves: SAVE PLACE, power cycle, CONTINUE restores seed and position;
     a corrupted newest slot falls back to the older one
 11  determinism: re-entering the same seed rebuilds identical streets
 12  long session: 30 emulated minutes of walking and listening
 13  audio: the ROM produces evolving sound on all four channels
Usage: emu_tests.py [rom] [--quick]   writes build/emu_report.json
"""
import json
import random
import re
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/validation'))
from emu import Emu  # noqa: E402
import savefile  # noqa: E402

QUICK = '--quick' in sys.argv
RESULTS = []


def record(name, ok, detail):
    RESULTS.append({'test': name, 'ok': bool(ok), 'detail': detail})
    print(f'[{"PASS" if ok else "FAIL"}] {name}: {detail}')


# ------------------------------------------------------------ host oracle
def c_array(src, name):
    m = re.search(name + r'\[\] = \{(.*?)\};', src, re.S)
    return [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(1))]


RD = (ROOT / 'build/gen/render_data.c').read_text()
MT_TILES, MT_ATTRS, MT_FLAGS = c_array(RD, 'mt_tiles'), c_array(RD, 'mt_attrs'), c_array(RD, 'rmt_flags')
HOST = ROOT / 'build/host_world'


def host_region(seed, x0, y0, w, h):
    out = ROOT / 'build/emu/region.bin'
    subprocess.run([str(HOST), str(out), str(seed), str(x0), str(y0), str(w), str(h)],
                   check=True, capture_output=True)
    d = out.read_bytes()
    return [[d[y * w + x] for x in range(w)] for y in range(h)]


def check_view(e, label):
    """Compare the hardware BG map (both VRAM banks) with the host generator."""
    seed, cx, cy = e.u16('world_seed'), e.u16('cam_x'), e.u16('cam_y')
    if e.u8('train_active'):
        e.tick(200)
    region = host_region(seed, cx, cy, 11, 10)
    m = e.pb.memory
    bad = 0
    for j in range(10):
        for i in range(11):
            mt = region[j][i]
            wx, wy = (cx + i) & 0xFFFF, (cy + j) & 0xFFFF
            for q in range(4):
                tx = ((wx & 15) * 2 + (q & 1)) & 31
                ty = ((wy & 15) * 2 + (q >> 1)) & 31
                tile = m[0, 0x9800 + ty * 32 + tx]
                attr = m[1, 0x9800 + ty * 32 + tx]
                et, ea = MT_TILES[mt * 4 + q], MT_ATTRS[mt * 4 + q]
                mask = 0xF8 if MT_FLAGS[mt] & 2 else 0xFF   # blinking lights change palette
                if tile != et or (attr & mask) != (ea & mask):
                    bad += 1
    return bad


def boot_new_city(sram=None, sound=False):
    e = Emu(sram=sram, sound=sound)
    e.tick(150)
    e.press('start', 8, 8)
    if sram is not None:            # title offers CONTINUE / NEW CITY
        e.tick(20)
        e.press('a', 8, 8)
    e.tick(200)
    return e


def open_page(e, page, row=0):
    """Open the synthesiser on a given page and row (the menu remembers both)."""
    e.tick(20)
    e.press('start')
    e.tick(4)
    while e.u8('menu_page') != page:
        e.press('select')
    while e.u8('menu_sel') != row:
        e.press('down')


def solid_at(seed, x, y):
    return MT_FLAGS[host_region(seed, x, y, 1, 1)[0][0]] & 1


# ----------------------------------------------------------------- tests
def t_boot():
    from PIL import Image
    e = Emu()
    e.tick(160)
    scr = np.asarray(e.pb.screen.image.convert('RGB'), dtype=np.int16)
    ref = np.asarray(Image.open(ROOT / 'assets/interface/title.png').convert('RGB'), dtype=np.int16)
    diff = np.abs(scr[:128] - ref[:128]).mean()   # bottom rows hold the blinking prompt
    e.shot(ROOT / 'build/emu/title.png', 2)
    record('boot/title', diff < 10, f'mean colour difference from title picture {diff:.2f} (rows 0-127)')
    e.stop()


def t_new_city_and_streaming():
    rnd = random.Random(7)
    e = boot_new_city()
    seed, x, y = e.u16('world_seed'), e.u16('pl_x'), e.u16('pl_y')
    record('new city', not solid_at(seed, x, y), f'seed {seed}, walker at ({x},{y}) on a walkable metatile')
    total_bad = checks = steps = on_solid = 0
    dirs = ['up', 'down', 'left', 'right']
    walks = 40 if QUICK else 160
    for k in range(walks):
        d = rnd.choice(dirs)
        n = rnd.randint(1, 12)
        e.pb.button_press(d)
        if rnd.random() < 0.5:
            e.pb.button_press('b')
        e.tick(n * 16)
        e.pb.button_release(d)
        e.pb.button_release('b')
        e.tick(18)
        steps += n
        px, py = e.u16('pl_x'), e.u16('pl_y')
        on_solid += solid_at(seed, px, py)
        if k % 4 == 0:
            total_bad += check_view(e, k)
            checks += 1
    record('streaming', total_bad == 0,
           f'{checks} full-view comparisons after {walks} random walks: {total_bad} mismatched map entries')
    record('collision', on_solid == 0, f'walker on a solid metatile {on_solid} times')
    e.shot(ROOT / 'build/emu/walk.png', 2)
    e.stop()


def t_seam():
    seed = 4242
    # put the walker just left of and above the seam, on a walkable tile
    for dx in range(0, 40):
        x, y = 65530 - dx, 65533
        if not solid_at(seed, x, y):
            break
    rec = savefile.build(world_seed=seed, px=x, py=y)
    e = boot_new_city(sram=savefile.sram_with(rec))
    sx, sy = e.u16('pl_x'), e.u16('pl_y')
    crossed_x = crossed_y = False
    bad = 0
    for _ in range(30):
        for d in ('right', 'down'):
            e.pb.button_press(d)
            e.tick(16 * 3)
            e.pb.button_release(d)
            e.tick(18)
            if e.u16('pl_x') < 1000:
                crossed_x = True
            if e.u16('pl_y') < 1000:
                crossed_y = True
        if crossed_x or crossed_y:
            bad += check_view(e, 'seam')
    record('world seam', (crossed_x or crossed_y) and bad == 0,
           f'start ({sx},{sy}); crossed x seam {crossed_x}, y seam {crossed_y}; '
           f'map mismatches {bad}; now ({e.u16("pl_x")},{e.u16("pl_y")})')
    e.stop()


def t_listening_and_menu():
    e = boot_new_city()
    x, y, s0 = e.u16('pl_x'), e.u16('pl_y'), e.u16('music_steps')
    e.press('select')
    for d in ('left', 'right', 'up', 'down'):
        e.hold(d, 40)
    e.tick(300)
    moved = (e.u16('pl_x'), e.u16('pl_y')) != (x, y)
    s1 = e.u16('music_steps')
    record('listening mode', not moved and e.u8('pl_listening') == 1 and s1 > s0,
           f'walker still: {not moved}; music advanced {s1 - s0} steps during 8 s')
    e.press('select')
    seen = set()
    for d in ('down', 'right', 'up', 'left'):
        e.hold(d, 40)
        e.tick(20)
        seen.add((e.u16('pl_x'), e.u16('pl_y')))
    record('listening exit', len(seen - {(x, y)}) > 0, f'walking resumes after SELECT ({len(seen)} positions visited)')
    # synthesiser: raise DRONE, lower MELODY; music keeps running in the menu
    mp = e.sym['_mparam']
    before = [e.pb.memory[mp + i] for i in range(19)]
    open_page(e, 0, 0)
    s2 = e.u16('music_steps')
    e.press('left')                  # DRONE -1
    e.press('down')
    e.press('left'); e.press('left')  # MELODY -2
    e.tick(240)
    after = [e.pb.memory[mp + i] for i in range(19)]
    s3 = e.u16('music_steps')
    ok = after[0] == max(0, before[0] - 1) and after[1] == max(0, before[1] - 2) and s3 > s2
    e.shot(ROOT / 'build/emu/menu.png', 2)
    record('synthesiser edits', ok, f'DRONE {before[0]}->{after[0]}, MELODY {before[1]}->{after[1]}, '
                                     f'{s3 - s2} music steps while the menu was open')
    # presets: MEMORY page, LOAD PRESET, cycle through the five factory presets
    while e.u8('menu_page') != 4:
        e.press('select')
    while e.u8('menu_sel') != 1:
        e.press('down')
    names = []
    okp = True
    expected = [[11, 10, 8, 6, 9, 0, 7, 4], [12, 6, 11, 10, 4, 2, 6, 8], [12, 7, 6, 5, 1, 7, 2, 11],
                [9, 10, 10, 6, 2, 1, 9, 5], [8, 11, 7, 3, 5, 6, 12, 2]]
    for p in range(5):
        if p:
            e.press('right')
        e.press('a')
        e.tick(10)
        got = [e.pb.memory[mp + i] for i in range(8)]
        okp &= got == expected[p]
        names.append(got[5])
    record('presets', okp, f'five factory presets loaded (modes {names})')
    # weather lock: WORLD page, WEATHER -> HEAVY RAIN
    while e.u8('menu_page') != 3:
        e.press('select')
    while e.u8('menu_sel') != 1:
        e.press('down')
    for _ in range(3):
        e.press('right')
    e.press('start')
    e.tick(60 * 20)
    rain = e.u8('music_rain')
    oam = [e.pb.memory[0xFE00 + 4 * i] for i in range(2, 12)]
    visible = sum(1 for v in oam if 0 < v < 160)
    e.shot(ROOT / 'build/emu/rain.png', 2)
    record('weather lock', e.u8('weather_mode') == 3 and rain >= 10 and visible >= 4,
           f'mode {e.u8("weather_mode")} (3 = heavy rain), rain level {rain}, {visible} rain sprites on screen')
    e.stop()


def t_saves():
    e = boot_new_city()
    # walk a little, then WORLD -> SAVE PLACE
    e.hold('down', 48)
    e.hold('right', 48)
    e.tick(20)
    seed, x, y = e.u16('world_seed'), e.u16('pl_x'), e.u16('pl_y')
    open_page(e, 3, 5)
    e.press('a')
    e.tick(30)
    e.press('start')
    e.tick(30)
    ram = e.sram()
    a = savefile.parse(ram[0:savefile.SIZE])
    b = savefile.parse(ram[0x100:0x100 + savefile.SIZE])
    stored = a if a['valid'] else b
    record('save write', stored['valid'] and (stored['world_seed'], stored['px'], stored['py']) == (seed, x, y),
           f'slot A valid {a["valid"]}, slot B valid {b["valid"]}; stored seed {stored["world_seed"]} '
           f'at ({stored["px"]},{stored["py"]})')
    # power cycle and CONTINUE
    e2 = Emu(sram=ram)
    e2.tick(150)
    e2.press('start')
    e2.tick(10)
    e2.press('a')
    e2.tick(200)
    got = (e2.u16('world_seed'), e2.u16('pl_x'), e2.u16('pl_y'))
    record('save restore', got == (seed, x, y), f'after power cycle: seed {got[0]} at ({got[1]},{got[2]})')
    # save twice more (both slots used), then corrupt the newest slot
    e2.hold('left', 40)
    e2.tick(20)
    x2, y2 = e2.u16('pl_x'), e2.u16('pl_y')
    open_page(e2, 3, 5)
    e2.press('a')
    e2.tick(30)
    e2.press('start')
    ram2 = bytearray(e2.sram())
    sa = savefile.parse(bytes(ram2[0:savefile.SIZE]))
    sb = savefile.parse(bytes(ram2[0x100:0x100 + savefile.SIZE]))
    newest = 0 if (sa['valid'] and (not sb['valid'] or ((sa['seq'] - sb['seq']) & 255) < 128)) else 0x100
    ram2[newest + 20] ^= 0x55
    e3 = Emu(sram=bytes(ram2))
    e3.tick(150)
    e3.press('start')
    e3.tick(10)
    e3.press('a')
    e3.tick(220)
    got3 = (e3.u16('pl_x'), e3.u16('pl_y'))
    record('corrupted save', got3 == (x, y) and e3.u8('save_status') == 2,
           f'newest slot damaged -> older place ({got3[0]},{got3[1]}), expected ({x},{y}); '
           f'status {e3.u8("save_status")} (2 = recovered, popup shown)')
    e3.shot(ROOT / 'build/emu/recovered.png', 2)
    e3.stop()
    # blank / garbage SRAM boots straight to a new city
    e4 = Emu(sram=bytes(random.Random(3).getrandbits(8) for _ in range(8192)))
    e4.tick(150)
    e4.press('start')
    e4.tick(200)
    record('garbage SRAM', e4.u16('pl_x') != 0 and e4.u8('save_status') == 0,
           f'random SRAM ignored, new city at ({e4.u16("pl_x")},{e4.u16("pl_y")})')
    e4.stop()


def t_determinism():
    seed = 31337
    rec = savefile.build(world_seed=seed, px=0, py=0, has_place=0)
    e = boot_new_city()
    # WORLD -> SEED edit: type 31337
    open_page(e, 3, 0)
    e.press('a')
    digits = [int(c) for c in f'{seed:05d}']
    cur = [int(c) for c in f'{e.u16("world_seed"):05d}']
    for k in range(5):
        delta = (digits[k] - cur[k]) % 10
        for _ in range(delta):
            e.press('up', 3, 3)
        e.press('right', 3, 3)
    e.press('a')
    e.tick(200)
    s1 = e.u16('world_seed')
    first = (e.u16('pl_x'), e.u16('pl_y'))
    bad1 = check_view(e, 'seed')
    # a different city, then the same seed again
    open_page(e, 3, 4)
    e.press('a')
    e.tick(200)
    other = e.u16('world_seed')
    open_page(e, 3, 0)
    e.press('a')
    cur = [int(c) for c in f'{other:05d}']
    for k in range(5):
        delta = (digits[k] - cur[k]) % 10
        for _ in range(delta):
            e.press('up', 3, 3)
        e.press('right', 3, 3)
    e.press('a')
    e.tick(200)
    second = (e.u16('pl_x'), e.u16('pl_y'))
    bad2 = check_view(e, 'seed2')
    record('seed entry and determinism', s1 == seed and first == second and bad1 == 0 and bad2 == 0,
           f'typed seed {s1}; start {first} then {second} after visiting city {other}; mismatches {bad1}+{bad2}')
    del rec
    e.stop()


def t_long_session():
    rnd = random.Random(99)
    e = boot_new_city()
    minutes = 5 if QUICK else 30
    t0 = time.time()
    steps0 = e.u16('music_steps')
    fc_ok = True
    sp_min = 0xFFFF
    for m in range(minutes):
        for _ in range(6):
            d = rnd.choice(['up', 'down', 'left', 'right', None, None])
            if d:
                e.pb.button_press(d)
                e.pb.button_press('b')
            e.tick(600)
            if d:
                e.pb.button_release(d)
                e.pb.button_release('b')
            sp = e.pb.register_file.SP
            sp_min = min(sp_min, sp)
        a = e.u8('frame_count')
        e.tick(10)
        fc_ok &= ((e.u8('frame_count') - a) & 255) == 10
    s = (e.u16('music_steps') - steps0) & 0xFFFF
    bad = check_view(e, 'long')
    record('long session', fc_ok and bad == 0 and s > minutes * 150 and sp_min > 0xC000,
           f'{minutes} emulated minutes: interrupt every frame {fc_ok}, music steps {s}, '
           f'lowest SP 0x{sp_min:04X}, final view mismatches {bad} ({time.time() - t0:.0f} s wall)')
    e.stop()


def t_audio():
    e = Emu(sound=True)
    e.tick(150)
    e.press('start', 8, 8)
    frames = 60 * (30 if QUICK else 90)
    chans = {k: 0 for k in ('NR12', 'NR22', 'NR32', 'NR42')}
    regs = {'NR12': 0xFF12, 'NR22': 0xFF17, 'NR32': 0xFF1C, 'NR42': 0xFF21}
    buf = []
    for i in range(frames):
        e.pb.tick(1, True, True)
        buf.append(e.pb.sound.ndarray.copy())
        if i % 4 == 0:
            for k, a in regs.items():
                chans[k] += e.pb.memory[a] != 0
    x = np.concatenate(buf).astype(np.float32)
    x -= x.mean(axis=0)
    seg = 48000
    rms = [float(np.sqrt((x[i:i + seg] ** 2).mean())) for i in range(0, len(x) - seg, seg)]
    active = sum(1 for r in rms if r > 0.05)
    # spectral change between successive 2-second windows
    specs = []
    for i in range(0, len(x) - 96000, 96000):
        f = np.abs(np.fft.rfft(x[i:i + 96000, 0]))[:4000]
        specs.append(f / (f.sum() + 1e-9))
    change = float(np.mean([np.abs(specs[i + 1] - specs[i]).sum() for i in range(len(specs) - 1)]))
    trig = e.u16('snd_triggers')
    ok = active > len(rms) * 0.8 and all(v > 0 for v in chans.values()) and change > 0.2
    record('audio', ok, f'{frames / 60:.0f} s captured: sound in {active}/{len(rms)} seconds, '
                        f'{trig} voice triggers, channel activity {chans}, mean spectral change {change:.2f}')
    e.stop()


if __name__ == '__main__':
    start = time.time()
    t_boot()
    t_new_city_and_streaming()
    t_seam()
    t_listening_and_menu()
    t_saves()
    t_determinism()
    t_audio()
    t_long_session()
    n_ok = sum(r['ok'] for r in RESULTS)
    print(f'emu_tests: {n_ok}/{len(RESULTS)} passed in {time.time() - start:.0f} s')
    (ROOT / 'build/emu_report.json').write_text(json.dumps(
        {'passed': n_ok, 'total': len(RESULTS), 'quick': QUICK, 'results': RESULTS}, indent=1))
    sys.exit(0 if n_ok == len(RESULTS) else 1)
