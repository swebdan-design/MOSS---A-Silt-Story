#!/usr/bin/env python3
"""Record the running VACANT ROM's own audio (PyBoy CGB emulation).

  capture_rom_audio.py out.wav [minutes] [--walk] [--preset N] [--spectrogram out.png]

The ROM boots, starts a new city with a fixed seed (via a crafted save so the
recording is reproducible), optionally walks a scripted route through several
districts, and the emulator's 48 kHz stereo mix is written as 16-bit WAV.
Nothing is synthesised on the PC: every sample comes from the emulated
Game Boy Color sound hardware driven by the ROM.
"""
import argparse
import sys
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/validation'))
from emu import Emu  # noqa: E402
import savefile  # noqa: E402

PRESETS = [
    [11, 10, 8, 6, 9, 0, 7, 4, 6, 5, 6, 10, 8, 10, 0, 0, 4, 1, 1],
    [12, 6, 11, 10, 4, 2, 6, 8, 10, 3, 9, 12, 10, 10, 1, 4, 8, 0, 0],
    [12, 7, 6, 5, 1, 7, 2, 11, 3, 6, 3, 9, 6, 12, 3, 2, 8, 2, 2],
    [9, 10, 10, 6, 2, 1, 9, 5, 8, 7, 8, 6, 9, 9, 2, 1, 2, 1, 1],
    [8, 11, 7, 3, 5, 6, 12, 2, 4, 8, 4, 4, 5, 8, 0, 3, 4, 2, 2],
]
PRESET_SEEDS = [0x7A31, 0x0BED, 0x51C4, 0x24F7, 0x6D02]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('minutes', nargs='?', type=float, default=3.0)
    ap.add_argument('--walk', action='store_true')
    ap.add_argument('--preset', type=int, default=0)
    ap.add_argument('--seed', type=int, default=1)
    ap.add_argument('--weather', type=int, default=0)
    ap.add_argument('--spectrogram')
    a = ap.parse_args()

    # a reproducible start: crafted save with the chosen city, place and preset
    import subprocess
    pos = ROOT / 'build/emu/start.bin'
    pos.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(ROOT / 'build/host_world'), str(pos), str(a.seed), '0', '0', '1', '1'],
                   check=True, capture_output=True, text=True)
    out = subprocess.run([str(ROOT / 'build/host_world'), str(pos), str(a.seed), '0', '0', '1', '1'],
                         check=True, capture_output=True, text=True).stdout
    x, y = [int(v) for v in out.split(' at ')[1].split(',')]
    rec = savefile.build(world_seed=a.seed, px=x, py=y, params=PRESETS[a.preset],
                         music_seed=PRESET_SEEDS[a.preset], weather_mode=a.weather)
    e = Emu(sram=savefile.sram_with(rec), sound=True)
    e.tick(150)
    e.press('start', 8, 8)
    e.tick(20)
    e.press('a', 8, 8)
    frames = int(a.minutes * 60 * 59.73)
    route = ['right'] * 40 + ['down'] * 30 + ['right'] * 50 + ['up'] * 30 + ['left'] * 20 + ['down'] * 60
    chunks = []
    step = 0
    for f in range(frames):
        if a.walk and f % 20 == 0 and f > 400:
            for k in ('up', 'down', 'left', 'right'):
                e.pb.button_release(k)
            if step < len(route) and (f // 600) % 3 != 2:      # walk, walk, pause
                e.pb.button_press(route[step % len(route)])
                step += 1
        e.pb.tick(1, False, True)
        chunks.append(e.pb.sound.ndarray.copy())
    x = np.concatenate(chunks).astype(np.float32)
    x -= x.mean(axis=0)
    peak = np.abs(x).max() or 1.0
    x = x / peak * 0.89
    with wave.open(a.out, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(e.pb.sound.sample_rate)
        w.writeframes((x * 32767).astype('<i2').tobytes())
    print(f'wrote {a.out}: {len(x) / 48000:.1f} s, {e.u16("snd_triggers")} voice triggers, '
          f'{e.u16("music_steps")} music steps')
    if a.spectrogram:
        from PIL import Image
        mono = x.mean(axis=1)
        win, hop = 4096, 2400
        cols = []
        for i in range(0, len(mono) - win, hop):
            s = np.abs(np.fft.rfft(mono[i:i + win] * np.hanning(win)))[:700]
            cols.append(np.log1p(s * 40))
        sp = np.array(cols).T[::-1]
        sp = (255 * sp / sp.max()).astype(np.uint8)
        img = Image.fromarray(sp).resize((min(1600, sp.shape[1] * 2), 350))
        img.save(a.spectrogram)
    e.stop()


if __name__ == '__main__':
    main()
