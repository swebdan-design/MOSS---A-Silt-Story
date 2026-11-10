"""Full-screen and notebook pictures for MOSS, drawn in code (stdlib only).

Shades: 0 white, 1 light grey, 2 dark grey, 3 black. Every picture is built
from primitives here so it can be regenerated and edited. Returns a dict
name -> (pixels, tile_budget) for tools/gen_assets.py.
"""
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_font():
    glyphs, lines = {}, (ROOT / 'assets/graphics/font.txt').read_text().splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith('== '):
            glyphs[lines[i][3:]] = [l.strip() for l in lines[i + 1:i + 8]]
            i += 8
        else:
            i += 1
    return glyphs


FONT = load_font()


class Canvas:
    def __init__(self, w, h, fill=0):
        self.w, self.h = w, h
        self.px = [[fill] * w for _ in range(h)]

    def set(self, x, y, c):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[y][x] = c

    def rect(self, x, y, w, h, c):
        for yy in range(int(y), int(y + h)):
            for xx in range(int(x), int(x + w)):
                self.set(xx, yy, c)

    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / max(1, n)
            self.set(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c)

    def ellipse(self, cx, cy, rx, ry, c, fill=True):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                if fill and d <= 1.0:
                    self.set(x, y, c)
                elif not fill and 0.80 <= d <= 1.15:
                    self.set(x, y, c)

    def ring(self, cx, cy, r, c, ry=None):
        ry = ry or r
        steps = int(2 * math.pi * max(r, ry) * 1.5) + 8
        for i in range(steps):
            a = 2 * math.pi * i / steps
            self.set(cx + r * math.cos(a), cy + ry * math.sin(a), c)

    def text(self, x, y, s, c, scale=1):
        for ch in s:
            g = FONT.get(ch)
            if ch == ' ' or g is None:
                x += 3 * scale
                continue
            w = max([max([i + 1 for i, v in enumerate(r) if v == '#'] or [0]) for r in g] or [1])
            for ry, r in enumerate(g):
                for rx, v in enumerate(r):
                    if v == '#':
                        self.rect(x + rx * scale, y + ry * scale, scale, scale, c)
            x += (w + 1) * scale

    def text_width(self, s, scale=1):
        w = 0
        for ch in s:
            g = FONT.get(ch)
            if ch == ' ' or g is None:
                w += 3 * scale
                continue
            gw = max([max([i + 1 for i, v in enumerate(r) if v == '#'] or [0]) for r in g] or [1])
            w += (gw + 1) * scale
        return w - scale

    def ctext(self, y, s, c, scale=1, x0=0, w=None):
        w = w or self.w
        self.text(x0 + (w - self.text_width(s, scale)) // 2, y, s, c, scale)


def willow(cv, x, y, h, c=3):
    """A drooping willow silhouette: trunk, crown, hanging strands."""
    cv.rect(x - 2, y - h // 2, 5, h // 2, c)
    cv.line(x, y - h // 2, x - 6, y - h + 6, c)
    cv.line(x + 1, y - h // 2, x + 8, y - h + 4, c)
    cv.ellipse(x, y - h + 4, 18, 9, c)
    for i, dx in enumerate(range(-17, 19, 3)):
        top = y - h + 6 + abs(dx) // 4
        length = h // 2 + 4 - abs(dx) // 2 + (i % 3) * 3
        for k in range(length):
            cv.set(x + dx + (k // 9) * (1 if dx > 0 else -1), top + k, c)


def boy_sitting(cv, x, y, c=3):
    """Small seated figure seen from behind (back of head, shoulders)."""
    cv.ellipse(x, y - 9, 3.2, 3.4, c)
    cv.rect(x - 4, y - 6, 9, 5, c)
    cv.rect(x - 5, y - 2, 11, 3, c)


def title_picture():
    cv = Canvas(160, 144, 3)
    # stars (deterministic)
    for (sx, sy) in [(12, 44), (28, 18), (140, 30), (150, 58), (20, 66), (114, 52), (60, 70), (98, 74), (6, 24), (132, 76)]:
        cv.set(sx, sy, 1)
    cv.set(28, 17, 2); cv.set(28, 19, 2); cv.set(27, 18, 2); cv.set(29, 18, 2)
    # crescent moon, upper right (SILT)
    cv.ellipse(136, 14, 8, 8, 0)
    cv.ellipse(140, 11, 7.5, 7.5, 3)
    # title: MOSS in the SILT alphabet, 4x, white
    cv.ctext(12, 'MOSS', 0, 4)
    cv.ctext(48, 'A SILT Story', 1)
    cv.ctext(58, 'by Synth Centric', 2)
    # horizon: distant wood, then a moonlit field
    for x in range(160):
        hgt = 6 + int(3 * math.sin(x / 7.0) + 2 * math.sin(x / 3.1))
        cv.rect(x, 82 - hgt, 1, hgt, 2)
    cv.rect(0, 82, 160, 62, 1)
    for x in range(0, 160, 9):
        cv.set(x + (x // 9) % 4, 86 + (x // 9) % 3, 2)
    # pond (black water) with a dark rim, two stones, rings
    cv.ellipse(100, 96, 46, 10, 2)
    cv.ellipse(100, 96, 44, 8.5, 3)
    cv.ring(86, 96, 10, 2, 3)
    cv.ring(86, 96, 18, 2, 5)
    cv.ellipse(86, 96, 3.5, 2, 1)
    cv.ellipse(116, 96, 3.5, 2, 1)
    cv.ring(116, 96, 9, 2, 3)
    for lx, ly in [(70, 93), (130, 99), (104, 92)]:
        cv.ellipse(lx, ly, 3, 1.2, 2)
    # willow on the left bank, the boy on the near bank
    willow(cv, 26, 106, 38, 3)
    boy_sitting(cv, 58, 108, 3)
    for x in range(40, 50):
        cv.set(x, 104 - (x % 3), 2)
    return cv.px


def ending_picture():
    cv = Canvas(160, 144, 0)
    cv.ctext(18, 'MOSS', 3, 4)
    cv.ctext(56, 'A SILT Story', 2)
    cv.ctext(68, 'by Synth Centric', 3)
    # the pond, small; two stones in it and one more on the bank
    cv.ellipse(80, 108, 40, 10, 2)
    cv.ellipse(80, 108, 38, 8.5, 3)
    cv.ellipse(68, 108, 3.5, 2, 1)
    cv.ellipse(94, 108, 3.5, 2, 1)
    cv.ring(68, 108, 11, 2, 3)
    cv.ellipse(84, 124, 2.5, 1.6, 2)   # the stone he left
    willow(cv, 24, 112, 34, 2)
    for x in range(0, 160, 7):
        cv.set(x + 3, 132, 1); cv.set(x, 136, 1)
    return cv.px


def icon(kind):
    cv = Canvas(32, 32, 0)
    if kind == 'wind':
        for i, x in enumerate((6, 11, 16, 21, 26)):
            for k in range(22):
                bend = int((k / 22) ** 2 * (5 + i))
                cv.set(x + bend, 29 - k, 3 if i % 2 else 2)
            cv.ellipse(x + 5 + i, 7 + (i % 2) * 2, 1.5, 3, 2)
        for y in (4, 9, 14):
            cv.line(1, y, 7, y - 1, 1)
    elif kind == 'water':
        cv.rect(0, 14, 32, 18, 2)
        for x, c in ((6, 1), (16, 1), (26, 1)):
            cv.ellipse(x, 21, 4, 2.5, 0)
            cv.ring(x, 21, 6, 1, 3.5)
        cv.line(0, 28, 31, 28, 1)
        for x in range(2, 30, 6):
            cv.set(x, 17, 0)
    elif kind == 'insects':
        for x in range(2, 31, 3):
            cv.line(x, 31, x + (x % 5) - 2, 18 + (x % 7), 2)
        cv.ellipse(15, 12, 6, 3, 3)
        cv.ellipse(9, 11, 2, 2, 3)
        cv.line(17, 10, 25, 2, 3); cv.line(19, 10, 28, 5, 3)
        cv.line(11, 14, 6, 19, 3); cv.line(16, 14, 14, 19, 3); cv.line(20, 13, 24, 18, 3)
        cv.set(8, 10, 0)
    elif kind == 'wood':
        cv.ellipse(16, 16, 14, 12, 3)
        cv.ellipse(16, 16, 12, 10, 2)
        cv.ring(16, 16, 9, 1, 7.5)
        cv.ring(16, 16, 6, 3, 5)
        cv.ellipse(16, 16, 3.5, 3, 3)
        cv.line(4, 5, 10, 9, 1)
    elif kind == 'rain':
        for x in range(0, 32):
            cv.set(x, 9 - abs(16 - x) // 3, 3)
        cv.rect(2, 9, 28, 2, 3)
        cv.rect(4, 11, 2, 18, 2); cv.rect(26, 11, 2, 18, 2)
        for i, (x, y) in enumerate([(9, 15), (14, 21), (19, 14), (23, 24), (11, 27), (17, 28)]):
            cv.line(x, y, x - 1, y + 3, 1 + i % 2)
        for x in (3, 10, 17, 24):
            cv.line(x + 2, 0, x, 4, 2)
    elif kind == 'echo':
        cv.ellipse(11, 14, 6, 7, 3)
        cv.rect(4, 17, 15, 4, 3)
        cv.ellipse(11, 22, 2, 1.5, 2)
        cv.rect(10, 2, 2, 6, 2)
        for r, c in ((10, 2), (15, 1), (20, 1)):
            for a in range(-40, 41, 4):
                t = math.radians(a)
                cv.set(11 + r * math.cos(t) + 2, 15 + r * math.sin(t), c)
    elif kind == 'still':
        for a in range(0, 360, 45):
            t = math.radians(a)
            x, y = 16 + 11 * math.cos(t), 20 + 6 * math.sin(t)
            cv.ellipse(x, y - 1, 2.5, 1.5, 3)
            cv.rect(x, y, 1, 3, 2)
        for x, y in ((16, 6), (9, 10), (23, 9), (13, 2)):
            cv.set(x, y, 2); cv.set(x + 1, y, 1); cv.set(x, y + 1, 1)
    elif kind == 'memory':
        cv.rect(4, 2, 24, 2, 3)
        for i, x in enumerate((7, 12, 17, 22, 26)):
            cv.line(x, 4, x, 7, 2)
            cv.rect(x - 1, 8, 3, 9 + (i % 3) * 3, 3 if i % 2 else 2)
        cv.line(16, 4, 16, 22, 2)
        cv.ellipse(16, 25, 4, 3, 1)
        cv.ring(16, 25, 4, 3, 3)
    return cv.px


def build():
    out = {'title': (title_picture(), 144), 'ending': (ending_picture(), 144)}
    for k in ('wind', 'water', 'insects', 'wood', 'rain', 'echo', 'still', 'memory'):
        out['note_' + k] = (icon(k), 18)
    return out
