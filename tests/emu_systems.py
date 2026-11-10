#!/usr/bin/env python3
"""System tests on the compiled ROM (PyBoy): saving, loading, damaged
cartridge RAM, repeated pond entry/exit, and a 25-minute autonomous pond
session with audio analysis. Writes build/qa/systems.json."""
import sys, os, io, json, random, wave, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import warnings; warnings.filterwarnings('ignore')
import numpy as np
from pyboy import PyBoy
import play, symbols, savefile
from pyboy_util import Recorder

ROM = sys.argv[1] if len(sys.argv) > 1 else 'release/MOSS_by_Synth_Centric.gb'
LONG_MIN = int(os.environ.get('MOSS_LONG_MINUTES', '25'))
OUT = 'build/qa'
os.makedirs(OUT, exist_ok=True)
results = []


def check(name, cond, detail=''):
    results.append({'check': name, 'result': 'PASS' if cond else 'FAIL', 'detail': str(detail)})
    print(('PASS ' if cond else 'FAIL ') + name + (f'  ({detail})' if detail else ''), flush=True)


class P(play.Player):
    def __init__(self, sram=None, sound=False):
        self.ram = io.BytesIO(bytes(sram) if sram is not None else bytes(0x2000))
        self.pb = PyBoy(ROM, window='null', sound_emulated=sound, ram_file=self.ram, log_level='ERROR')
        self.syms = symbols.load('build/moss.noi')
        self.rec = Recorder(self.pb) if sound else None

    def sram(self):
        out = io.BytesIO()
        self.pb.stop(save=True, ram_file=out)
        return out.getvalue()[:0x2000]


def title_items(p):
    # number of visible title menu rows = rows with text tiles in the window
    return p.pb.memory[0xFF40] & 0x20


# 1. Fresh cartridge (all 0xFF) and zeroed RAM: title offers only a new summer
for name, fill in (('blank 0xFF', 0xFF), ('zeroed', 0x00)):
    p = P(bytes([fill]) * 0x2000)
    p.tick(300)
    check(f'{name} SRAM boots to title without a save', p.pb.memory[0xFF47] == 0xE4 and p.u8('_save_status') in (0, 3),
          'status=%d' % p.u8('_save_status'))
    p.pb.stop(save=False)

# 2. Random garbage SRAM never crashes the boot
rng = random.Random(7)
ok = True
for i in range(12):
    p = P(bytes(rng.randrange(256) for _ in range(0x2000)))
    p.tick(240)
    if p.pb.memory[0xFF47] != 0xE4 or not (p.pb.memory[0xFF40] & 0x80):
        ok = False
    p.pb.stop(save=False)
check('12 random-garbage SRAM images: title still appears', ok)

# 3. Save from the pause menu, reload the cartridge, Continue
p = P(savefile.build(day=2, postgame=0, map_=1, x=7, y=7, flags=(0, 1, 2, 3, 4, 5, 33), found=0x08))
p.tick(300)
p.press('a', 3, 200)      # Continue
p.settle(200)
check('Continue restores day 2 in the village', p.pos()[0] == 1 and p.day() == 2, str(p.pos()))
p.goto(9, 7)
p.press('start', 3, 30); p.press('down', 3, 10); p.press('a', 3, 60)   # Save
p.settle(120)
sram = p.sram()
p = P(sram)
p.tick(300); p.press('a', 3, 200); p.settle(200)
check('saved position survives a power cycle', p.pos()[:3] == (1, 9, 7), str(p.pos()))
p.pb.stop(save=False)

# 4. Newest copy damaged: the older copy is used and the game still starts
img = bytearray(sram)
seqA, seqB = img[3], img[savefile.SLOT + 3]
newest = 0 if ((seqA - seqB) & 0xFF) < 128 and seqA != seqB else 1
off = newest * savefile.SLOT
img[off + 40:off + 80] = bytes(40)
p = P(img)
p.tick(300)
check('damaged newest save -> recovered from the other copy', p.u8('_save_status') == 2, 'status=%d' % p.u8('_save_status'))
p.tick(60); check('player is told about the recovery', p.dialog_up())
p.advance(); p.tick(150)
p.press('a', 3, 200); p.settle(200)
check('recovered save is playable', p.pos()[0] == 1 and p.day() == 2, str(p.pos()))
p.pb.stop(save=False)

# 5. Both copies damaged: title, no Continue, New summer still works
img2 = bytearray(img)
img2[(1 - newest) * savefile.SLOT + 50] ^= 0x77
p = P(img2)
p.tick(300)
check('both copies damaged -> reported, no crash', p.u8('_save_status') == 3)
p.tick(60); check('player is told the notebook could not be read', p.dialog_up())
p.advance(); p.tick(150)
p.press('a', 3, 120)
for _ in range(40):
    if p.pos()[0] == 0: break
    p.press('a', 3, 47)
check('new summer still starts after corruption', p.pos()[0] == 0 and p.day() == 1, str(p.pos()))
p.pb.stop(save=False)

# 6. Repeatedly entering and leaving the pond (post-game state, at the bank)
p = P(savefile.postgame())
p.tick(300); p.press('a', 3, 200); p.settle(200)
ok = True
for i in range(20):
    p.face('up'); p.press('a', 3, 40); p.press('a', 3, 20)
    for _ in range(60):
        if p.pos()[0] == 8: break
        p.tick(5)
    p.tick(30)
    inside = p.pos()[0] == 8
    for _ in range(4):                       # START, then 'Stand up'
        p.press('start', 3, 40); p.press('a', 3, 20)
        for _ in range(60):
            if p.pos()[0] == 3: break
            p.tick(5)
        if p.pos()[0] == 3: break
        p.press('b', 3, 20)
    p.tick(40); p.settle(40)
    if not inside or p.pos()[0] != 3:
        ok = False; print('  cycle', i, 'failed at', p.pos()); break
check('20 cycles of sitting at the pond and standing up', ok)
check('no audio queue leak after the cycles', p.u8('_audio_dropped') < 250, 'dropped=%d' % p.u8('_audio_dropped'))
p.pb.stop(save=False)

# 7. Long autonomous pond session with audio
p = P(savefile.postgame(), sound=True)
p.tick(300)
p.press('down', 3, 10); p.press('down', 3, 10); p.press('a', 3, 200)   # Return to the pond
p.press('select', 3, 10)                                                  # just listen
t0 = time.time()
frames = 3584 * LONG_MIN
p.rec.chunks = []
played0 = p.u16('_audio_played')
p.tick(frames)
a = p.rec.array().astype(np.float32)
mono = a.mean(axis=1) - a.mean()
rate = p.pb.sound.sample_rate
sec = rate
rms = np.array([np.sqrt((mono[i:i + sec] ** 2).mean()) for i in range(0, len(mono) - sec, sec)])
played = (p.u16('_audio_played') - played0) & 0xFFFF
silent = (rms < 0.15 * max(1e-6, np.median(rms))).mean()
check(f'{LONG_MIN} minutes of autonomous pond playback', len(rms) >= LONG_MIN * 60 - 5, f'{len(rms)} s captured')
check('pond keeps sounding (events dispatched throughout)', played > 60 * LONG_MIN, f'{played} hardware events (16-bit counter)')
quarters = [rms[i * len(rms) // 4:(i + 1) * len(rms) // 4].mean() for i in range(4)]
check('activity present in every quarter of the session', min(quarters) > 0.05 * max(quarters), str([round(q, 2) for q in quarters]))
p10, p90 = np.percentile(rms, 10), np.percentile(rms, 90)
check('the pond breathes: quiet and fuller passages (RMS p10/p90)', p10 < 0.8 * p90, f'{p10:.2f}/{p90:.2f}')
# loop detection: correlate 30-second loudness envelopes (50 ms frames)
hop = rate // 20
env = np.array([np.sqrt((mono[i:i + hop] ** 2).mean()) for i in range(0, len(mono) - hop, hop)])
W = 600
wins = [env[i:i + W] for i in range(0, len(env) - W + 1, W)]
best = 0.0
for i in range(len(wins)):
    for j in range(i + 1, len(wins)):
        a_, b_ = wins[i] - wins[i].mean(), wins[j] - wins[j].mean()
        c = float((a_ * b_).sum() / (np.sqrt((a_ ** 2).sum() * (b_ ** 2).sum()) + 1e-9))
        best = max(best, c)
check('no repeated 30-second passage (max envelope correlation < 0.9)', len(wins) < 2 or best < 0.9, f'{len(wins)} windows, max r={best:.2f}')
wav = f'{OUT}/long_pond_{LONG_MIN}min.wav'
i16 = np.clip(mono / (np.abs(mono).max() + 1e-9) * 20000, -32768, 32767).astype(np.int16)
with wave.open(wav, 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate); w.writeframes(i16.tobytes())
print('wrote', wav, 'in', round(time.time() - t0), 's')
p.pb.stop(save=False)

json.dump({'rom': ROM, 'emulator': 'PyBoy', 'results': results}, open(f'{OUT}/systems.json', 'w'), indent=1)
ok = all(r['result'] == 'PASS' for r in results)
print('\n%d/%d system checks passed' % (sum(r['result'] == 'PASS' for r in results), len(results)))
sys.exit(0 if ok else 1)
