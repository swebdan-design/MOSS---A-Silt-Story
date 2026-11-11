"""Small 2-bit pixel canvas used by the VACANT asset painters.

Shades are integers 0..3 (0 darkest). Sprites use 0 as transparent.
ASCII art uses '.'=0 ','=1 '+'=2 '#'=3; ' ' (space) means "leave as is".
Everything is deterministic: random textures take an explicit seed.
"""
import json
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASCII = {'.': 0, ',': 1, '+': 2, '#': 3, '0': 0, '1': 1, '2': 2, '3': 3}


class Canvas:
    def __init__(self, w=16, h=16, fill=0):
        self.w, self.h = w, h
        self.p = [[fill] * w for _ in range(h)]

    # ---- primitives
    def get(self, x, y):
        return self.p[y % self.h][x % self.w]

    def px(self, x, y, v):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y][x] = v
        return self

    def rect(self, x, y, w, h, v):
        for j in range(y, y + h):
            for i in range(x, x + w):
                self.px(i, j, v)
        return self

    def frame(self, x, y, w, h, v):
        self.hline(x, y, w, v).hline(x, y + h - 1, w, v)
        self.vline(x, y, h, v).vline(x + w - 1, y, h, v)
        return self

    def hline(self, x, y, w, v):
        return self.rect(x, y, w, 1, v)

    def vline(self, x, y, h, v):
        return self.rect(x, y, 1, h, v)

    def stamp(self, x, y, art):
        """Draw ASCII art (list of strings or a multi-line string)."""
        rows = art.strip('\n').split('\n') if isinstance(art, str) else art
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch in ASCII:
                    self.px(x + i, y + j, ASCII[ch])
        return self

    def speckle(self, seed, prob, v, where=None, mask=None):
        r = random.Random(seed)
        for y in range(self.h):
            for x in range(self.w):
                if r.random() < prob and (where is None or self.p[y][x] in where):
                    if mask is None or mask(x, y):
                        self.p[y][x] = v
        return self

    def replace(self, a, b, region=None):
        x0, y0, w, h = region or (0, 0, self.w, self.h)
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                if self.p[y][x] == a:
                    self.p[y][x] = b
        return self

    def blit(self, other, x, y, transparent=None):
        for j in range(other.h):
            for i in range(other.w):
                v = other.p[j][i]
                if transparent is None or v != transparent:
                    self.px(x + i, y + j, v)
        return self

    def sub(self, x, y, w, h):
        c = Canvas(w, h)
        for j in range(h):
            for i in range(w):
                c.p[j][i] = self.p[y + j][x + i]
        return c

    def copy(self):
        c = Canvas(self.w, self.h)
        c.p = [r[:] for r in self.p]
        return c

    def flipx(self):
        c = self.copy()
        c.p = [r[::-1] for r in c.p]
        return c

    def flipy(self):
        c = self.copy()
        c.p = c.p[::-1]
        return c

    def shift(self, dx, dy):
        c = Canvas(self.w, self.h)
        for y in range(self.h):
            for x in range(self.w):
                c.p[y][x] = self.p[(y - dy) % self.h][(x - dx) % self.w]
        return c


def hex2rgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def load_palettes():
    return json.loads((ROOT / 'assets/palettes/palettes.json').read_text())


GRAY = [(0, 0, 0), (85, 85, 85), (170, 170, 170), (255, 255, 255)]


def save_gray_png(path, canvases, cols, cell_w, cell_h, scale=1):
    """Write canvases into a sheet as 4-level grayscale (the editable source)."""
    from PIL import Image
    rows = (len(canvases) + cols - 1) // cols
    img = Image.new('RGB', (cols * cell_w, max(1, rows) * cell_h), (255, 0, 255))
    for n, c in enumerate(canvases):
        ox, oy = (n % cols) * cell_w, (n // cols) * cell_h
        for y in range(c.h):
            for x in range(c.w):
                img.putpixel((ox + x, oy + y), GRAY[c.p[y][x]])
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    img.save(path)


def gray_to_shade(rgb):
    v = rgb[0]
    return min(3, (v + 42) // 85)
