"""Scriptable MOSS session on PyBoy: input, screenshots and game-state reads
through the linker's symbol table. Development/QA only."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from pyboy_util import boot, Recorder
import symbols

BTN = {'a': 'a', 'b': 'b', 'start': 'start', 'select': 'select',
       'up': 'up', 'down': 'down', 'left': 'left', 'right': 'right'}
DIRS = {'down': 0, 'up': 1, 'left': 2, 'right': 3}


class Session:
    def __init__(self, rom, sound=False, noi='build/moss.noi', sav=None):
        self.pb = boot(rom, sound=sound)
        self.syms = symbols.load(noi)
        self.rec = Recorder(self.pb) if sound else None
        self.shots = 0

    def tick(self, n=1):
        if self.rec: self.rec.tick(n)
        else:
            for _ in range(n): self.pb.tick(1, False, False)

    def sym(self, name):
        return symbols.addr(self.syms, name) & 0xFFFF

    def u8(self, name, off=0):
        return self.pb.memory[self.sym(name) + off]

    def u16(self, name, off=0):
        a = self.sym(name) + off
        return self.pb.memory[a] | self.pb.memory[a + 1] << 8

    def press(self, btn, hold=4, after=6):
        self.pb.button_press(BTN[btn])
        self.tick(hold)
        self.pb.button_release(BTN[btn])
        self.tick(after)

    def hold(self, btn, frames):
        self.pb.button_press(BTN[btn])
        self.tick(frames)
        self.pb.button_release(BTN[btn])
        self.tick(2)

    def shot(self, path):
        self.pb.tick(1, True, self.rec is not None)
        self.pb.screen.image.convert('RGB').save(path)

    # game state ------------------------------------------------------------
    def pos(self):
        return self.u8('_cur_map'), self.u8('_px'), self.u8('_py'), self.u8('_pdir')

    def window_up(self):
        return bool(self.pb.memory[0xFF40] & 0x20)
