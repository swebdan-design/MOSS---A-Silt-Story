"""Thin PyBoy wrapper for VACANT tests and captures (CGB mode, no window)."""
import os
import sys
import warnings
from pathlib import Path

warnings.filterwarnings('ignore')
os.environ.setdefault('PYSDL2_DLL_PATH', '')
from pyboy import PyBoy  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
ROM = ROOT / 'release/VACANT_by_Synth_Centric.gbc'

KEYS = {'a': 'a', 'b': 'b', 'start': 'start', 'select': 'select',
        'up': 'up', 'down': 'down', 'left': 'left', 'right': 'right'}


class Emu:
    def __init__(self, rom=ROM, sram=None, sound=False):
        """Boot a scratch copy of the ROM. sram: bytes for battery RAM, or None
        for a blank cartridge. Battery RAM is never written next to the release."""
        import io
        import shutil
        run = ROOT / 'build/emu'
        run.mkdir(parents=True, exist_ok=True)
        self.rom = run / 'vacant_test.gbc'
        shutil.copy(rom, self.rom)
        ram = io.BytesIO(sram if sram is not None else b'\xff' * 8192)
        kw = dict(window='null', cgb=True, sound_emulated=sound, ram_file=ram)
        self.pb = PyBoy(str(self.rom), **kw)
        self.pb.set_emulation_speed(0)
        self.frames = 0
        self.sym = self._symbols()

    def _symbols(self):
        sym = {}
        m = ROOT / 'build/vacant.noi'
        if m.exists():
            for line in m.read_text().splitlines():
                p = line.split()
                if len(p) == 3 and p[0] == 'DEF':
                    sym[p[1]] = int(p[2], 16)
        return sym

    def tick(self, n=1, sound=False):
        for _ in range(n):
            self.pb.tick(1, True, sound)
            self.frames += 1

    def press(self, key, hold=6, after=6):
        self.pb.button_press(KEYS[key])
        self.tick(hold)
        self.pb.button_release(KEYS[key])
        self.tick(after)

    def hold(self, key, frames):
        self.pb.button_press(KEYS[key])
        self.tick(frames)
        self.pb.button_release(KEYS[key])

    def mem(self, addr, n=1):
        return [self.pb.memory[addr + i] for i in range(n)]

    def u8(self, name):
        return self.pb.memory[self.sym['_' + name]]

    def u16(self, name):
        a = self.sym['_' + name]
        return self.pb.memory[a] | (self.pb.memory[a + 1] << 8)

    def shot(self, path, scale=1):
        img = self.pb.screen.image.convert('RGB')
        if scale != 1:
            from PIL import Image
            img = img.resize((160 * scale, 144 * scale), Image.NEAREST)
        img.save(path)
        return img

    def sram(self):
        """Battery RAM contents as the cartridge would keep them."""
        import io
        buf = io.BytesIO()
        self.pb.stop(save=True, ram_file=buf)
        return buf.getvalue()

    def stop(self):
        self.pb.stop(save=False)
