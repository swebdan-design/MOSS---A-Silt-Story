#!/usr/bin/env python3
"""Record the MOSS companion soundtrack from the actual ROM.

Every track is the game's own audio, captured from the SameBoy emulator core
(DMG model, SameBoy's open-source boot ROM) while the ROM runs in a specific
state set up through a cartridge save and ordinary button input. Nothing is
re-synthesised. Post-processing is limited to DC removal, peak normalisation
and short fades at the start and end of each track.

Outputs WAV (48 kHz, 16-bit stereo) to build/soundtrack/ and, if ffmpeg is
present, FLAC and MP3 copies. Writes a provenance JSON.
Environment: SAMEBOY_LIB, SAMEBOY_BOOT (see tools/sameboy_backend.py).
"""
import os, sys, json, wave, subprocess, time
sys.path.insert(0, os.path.dirname(__file__))
os.environ['MOSS_EMU'] = 'sameboy'
import numpy as np
import play, savefile, symbols
from sameboy_backend import SameBoy
from pyboy_util import Recorder

ROM = 'release/MOSS_by_Synth_Centric.gb'
OUT = 'build/soundtrack'
os.makedirs(OUT, exist_ok=True)
FPS = 3584 / 60.0
SCALE = int(os.environ.get('MOSS_TRACK_SCALE', '100'))  # percent of nominal lengths (for quick tests)


class Rig(play.Player):
    def __init__(self, sram):
        self.pb = SameBoy(ROM, sound=True, sram=sram)
        self.syms = symbols.load('build/moss.noi')
        self.rec = Recorder(self.pb)


def overworld(day, map_, x, y, found, flags=()):
    base = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 22, 32, 33, 34, 35)
    return savefile.build(day=day, postgame=0, map_=map_, x=x, y=y, d=0, found=found,
                          flags=tuple(set(base) | set(flags)))


def continue_game(r):
    r.tick(420)
    r.press('a', 3, 240)
    r.settle(240)


def keep_awake(r, seconds, actions=None):
    """Stand still but tap B now and then so the boy isn't 'listening'
    (listening deliberately quietens the melody)."""
    frames = int(seconds * FPS * SCALE / 100)
    done = 0
    while done < frames:
        if actions and done in actions:
            actions[done](r)
        r.press('b', 2, 0)
        step = min(50, frames - done)
        r.tick(step)
        done += step + 2


def pond(preset):
    garden = [0] * 40
    return savefile.postgame(preset=preset, garden=garden)


def enter_pond(r):
    r.tick(420)
    r.press('down', 3, 10); r.press('down', 3, 10); r.press('a', 3, 200)
    r.press('select', 3, 30)   # just listen


def listen_pond(r, seconds):
    r.tick(int(seconds * FPS * SCALE / 100))


def knock_stones(r):
    for s in (6, 8, 4):
        r.goto(s, 6, face='up'); r.press('a', 3, 50); r.advance()


def ring_bell(r):
    r.goto(6, 6, face='up'); r.press('a', 3, 200); r.advance(); r.tick(40); r.advance()


TRACKS = [
    ('01', 'The Cottage', 'Mae\'s cottage on the second day: the kitchen clock, a few pebble phrases.',
     lambda: overworld(2, 0, 5, 6, 0x0F), lambda r: (continue_game(r), keep_awake(r, 170))),
    ('02', 'Whispering Wood', 'The wood after wind, hollow wood and insects have been found.',
     lambda: overworld(2, 2, 5, 9, 0x0F), lambda r: (continue_game(r), keep_awake(r, 190))),
    ('03', 'The Three Stones', 'The creek: the three stones knocked deep, high, in between; then the water.',
     lambda: overworld(2, 4, 6, 6, 0x0F), lambda r: (continue_game(r), knock_stones(r), keep_awake(r, 170))),
    ('04', 'Summer Rain', 'The pond instrument, preset Summer Rain, left alone.',
     lambda: pond(4), lambda r: (enter_pond(r), listen_pond(r, 240))),
    ('05', 'The Old Railway', 'The platform in the rain: the station bell and the cutting\'s echo, then absence.',
     lambda: overworld(3, 5, 6, 6, 0x3F), lambda r: (continue_game(r), ring_bell(r), keep_awake(r, 170))),
    ('06', 'The Hill', 'The hilltop in open fifths, crickets in the long grass.',
     lambda: overworld(2, 6, 6, 3, 0x0F), lambda r: (continue_game(r), keep_awake(r, 180))),
    ('07', 'The Pond at Evening', 'The pond instrument, preset In the Reeds.',
     lambda: pond(2), lambda r: (enter_pond(r), listen_pond(r, 240))),
    ('08', 'Before Leaving', 'The pond instrument, preset Before Leaving.',
     lambda: pond(6), lambda r: (enter_pond(r), listen_pond(r, 240))),
    ('09', 'The First Listener', 'The pond instrument, preset Quiet Clearing: stillness.',
     lambda: pond(5), lambda r: (enter_pond(r), listen_pond(r, 240))),
    ('10', 'The Pond Remains', 'The pond instrument with SILT\'s original twelve-lily garden.',
     lambda: pond(7), lambda r: (enter_pond(r), listen_pond(r, 360))),
]


def master(a, rate):
    x = a.astype(np.float64)
    x -= x.mean(axis=0)
    peak = np.abs(x).max() or 1.0
    x *= (0.891 * 32767) / peak                    # -1 dBFS peak
    fin, fout = int(2.0 * rate), int(5.0 * rate)
    x[:fin] *= np.linspace(0, 1, fin)[:, None]
    x[-fout:] *= np.linspace(1, 0, fout)[:, None]
    return np.clip(x, -32768, 32767).astype(np.int16)


def main():
    only = set(sys.argv[1:])
    prov = []
    for num, title, desc, mk_save, script in TRACKS:
        if only and num not in only: continue
        t0 = time.time()
        r = Rig(mk_save())
        r.rec.chunks = []
        script(r)
        a = r.rec.array()
        # drop the menu/boot portion before the scene settled: keep from 3 s in
        rate = 48000
        a = a[3 * rate:]
        m = master(a, rate)
        base = f'{OUT}/{num} - {title}'
        with wave.open(base + '.wav', 'wb') as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(rate); w.writeframes(m.tobytes())
        meta = ['-metadata', f'title={title}', '-metadata', 'artist=Synth Centric',
                '-metadata', 'album=MOSS (A SILT Story) Companion Soundtrack', '-metadata', f'track={int(num)}/10']
        for ext, args in (('flac', ['-c:a', 'flac']), ('mp3', ['-c:a', 'libmp3lame', '-q:a', '2'])):
            subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', base + '.wav'] + args + meta + [f'{base}.{ext}'], check=False)
        secs = len(m) / rate
        prov.append({'track': num, 'title': title, 'source': desc, 'seconds': round(secs, 1),
                     'emulator': 'SameBoy 1.0.1 core, DMG-B model', 'rom_sha256': __import__('hashlib').sha256(open(ROM, 'rb').read()).hexdigest()})
        print(f'{num} {title}: {secs:.0f} s captured in {time.time() - t0:.0f} s', flush=True)
    old = []
    if os.path.exists(f'{OUT}/provenance.json'):
        old = [t for t in json.load(open(f'{OUT}/provenance.json')) if t['track'] not in {p['track'] for p in prov}]
    json.dump(sorted(old + prov, key=lambda t: t['track']), open(f'{OUT}/provenance.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
