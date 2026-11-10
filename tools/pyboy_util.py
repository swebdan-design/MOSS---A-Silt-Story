"""Shared PyBoy helpers for MOSS QA and soundtrack capture (development only)."""
import os, sys, wave, warnings
warnings.filterwarnings('ignore')
os.environ.setdefault('PYSDL2_DLL_PATH', '')
import numpy as np
from pyboy import PyBoy


def boot(rom, sound=True, cgb=False):
    if os.environ.get('MOSS_EMU') == 'sameboy':
        from sameboy_backend import SameBoy
        return SameBoy(rom, sound=sound)
    return PyBoy(rom, window='null', sound_emulated=sound, cgb=cgb, log_level='ERROR',
                 sound_volume=100)


class Recorder:
    """Accumulates the emulator's per-frame stereo audio buffer."""
    def __init__(self, pb):
        self.pb = pb
        self.chunks = []

    def tick(self, n=1):
        for _ in range(n):
            self.pb.tick(1, False, True)
            a = self.pb.sound.ndarray
            if a is not None and len(a):
                self.chunks.append(np.array(a, copy=True))

    def array(self):
        if not self.chunks:
            return np.zeros((0, 2), dtype=np.int16)
        return np.concatenate(self.chunks)

    def save(self, path):
        a = self.array().astype(np.int32)
        if a.dtype != np.int16:
            peak = max(1, int(np.abs(a).max()))
            scale = 1 if a.max() > 255 else 256
            a = np.clip(a * scale, -32768, 32767).astype(np.int16)
        with wave.open(path, 'wb') as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(self.pb.sound.sample_rate)
            w.writeframes(a.tobytes())
        return a
