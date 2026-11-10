"""Minimal PyBoy-compatible facade over SameBoy's core (libsameboy.so) via
ctypes, so the same QA scripts can run on a second, independent emulator.
Set SAMEBOY_LIB to the shared library and SAMEBOY_BOOT to a DMG boot ROM
(SameBoy's own open-source dmg_boot.bin)."""
import ctypes, os
from PIL import Image

KEYS = {'right': 0, 'left': 1, 'up': 2, 'down': 3, 'a': 4, 'b': 5, 'select': 6, 'start': 7}
GB_MODEL_DMG_B = 0x002


class _Sample(ctypes.Structure):
    _fields_ = [('left', ctypes.c_int16), ('right', ctypes.c_int16)]


VBLANK_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int)
SAMPLE_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.POINTER(_Sample))
RGB_CB = ctypes.CFUNCTYPE(ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8)


class _Mem:
    def __init__(self, sb): self.sb = sb
    def __getitem__(self, a): return self.sb.lib.GB_safe_read_memory(self.sb.gb, a & 0xFFFF)


class _Screen:
    def __init__(self, sb): self.sb = sb
    @property
    def image(self):
        buf = bytes(self.sb.pixels)
        return Image.frombuffer('RGBX', (160, 144), buf, 'raw', 'BGRX', 0, 1).convert('RGB')


class _Sound:
    def __init__(self, sb): self.sb = sb; self.sample_rate = 48000
    @property
    def ndarray(self):
        import numpy as np
        out = np.array(self.sb.frame_samples, dtype=np.int16).reshape(-1, 2) if self.sb.frame_samples else None
        return out


class SameBoy:
    def __init__(self, rom, sound=False):
        self.lib = ctypes.CDLL(os.environ.get('SAMEBOY_LIB', 'libsameboy.so'))
        L = self.lib
        L.GB_alloc.restype = ctypes.c_void_p
        L.GB_init.restype = ctypes.c_void_p
        L.GB_init.argtypes = [ctypes.c_void_p, ctypes.c_int]
        L.GB_run.argtypes = [ctypes.c_void_p]
        L.GB_run_frame.argtypes = [ctypes.c_void_p]
        L.GB_run_frame.restype = ctypes.c_uint64
        L.GB_safe_read_memory.argtypes = [ctypes.c_void_p, ctypes.c_uint16]
        L.GB_safe_read_memory.restype = ctypes.c_uint8
        L.GB_set_key_state.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_bool]
        L.GB_load_rom.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.GB_load_boot_rom.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.GB_set_pixels_output.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        L.GB_set_vblank_callback.argtypes = [ctypes.c_void_p, VBLANK_CB]
        L.GB_set_rgb_encode_callback.argtypes = [ctypes.c_void_p, RGB_CB]
        L.GB_apu_set_sample_callback.argtypes = [ctypes.c_void_p, SAMPLE_CB]
        L.GB_set_sample_rate.argtypes = [ctypes.c_void_p, ctypes.c_uint]
        L.GB_save_battery.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        L.GB_load_battery.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.gb = L.GB_init(L.GB_alloc(), GB_MODEL_DMG_B)
        boot = os.environ.get('SAMEBOY_BOOT')
        if boot: L.GB_load_boot_rom(self.gb, boot.encode())
        self.pixels = (ctypes.c_uint32 * (160 * 144))()
        L.GB_set_pixels_output(self.gb, ctypes.addressof(self.pixels))
        self._vb = VBLANK_CB(self._on_vblank)
        self._rgb = RGB_CB(lambda g, r, gg, b: (r << 16) | (gg << 8) | b)
        L.GB_set_vblank_callback(self.gb, self._vb)
        L.GB_set_rgb_encode_callback(self.gb, self._rgb)
        self.frame_samples = []
        if sound:
            self._sc = SAMPLE_CB(self._on_sample)
            L.GB_set_sample_rate(self.gb, 48000)
            L.GB_apu_set_sample_callback(self.gb, self._sc)
        if L.GB_load_rom(self.gb, rom.encode()):
            raise RuntimeError('SameBoy could not load ' + rom)
        self.memory = _Mem(self)
        self.screen = _Screen(self)
        self.sound = _Sound(self)
        self.frame_count = 0
        self._vblank = False
        self.rom = rom

    def _on_vblank(self, gb, kind):
        self._vblank = True

    def _on_sample(self, gb, s):
        self.frame_samples.append(s.contents.left)
        self.frame_samples.append(s.contents.right)

    def tick(self, n=1, render=True, sound=False):
        for _ in range(n):
            self.frame_samples = []
            self.lib.GB_run_frame(self.gb)
            self.frame_count += 1
        return True

    def button_press(self, name): self.lib.GB_set_key_state(self.gb, KEYS[name], True)
    def button_release(self, name): self.lib.GB_set_key_state(self.gb, KEYS[name], False)
    def button(self, name): self.button_press(name); self.tick(4); self.button_release(name)
    def save_battery(self, path): return self.lib.GB_save_battery(self.gb, path.encode())
    def load_battery(self, path): return self.lib.GB_load_battery(self.gb, path.encode())
