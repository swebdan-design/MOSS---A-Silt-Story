#!/usr/bin/env python3
"""VACANT interface painter -> assets/interface/font.png/json, title.png/json.

Font: 8x8 cells, 5x7 glyphs in shade 3 with a shade-1 drop shadow, plus meter
segments and cursor glyphs. font.json 'chars' maps glyph index -> character
(meter glyphs use the stand-in characters listed there).

Title: a 160x144 night-city illustration painted against a per-tile palette
plan (title.json), so every 8x8 tile uses exactly one of eight GBC palettes.
"""
import json
import random
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from canvas import Canvas, ROOT, save_gray_png, hex2rgb  # noqa: E402

G5 = {
    'A': ".###. #...# #...# ##### #...# #...# #...#",
    'B': "####. #...# #...# ####. #...# #...# ####.",
    'C': ".###. #...# #.... #.... #.... #...# .###.",
    'D': "####. #...# #...# #...# #...# #...# ####.",
    'E': "##### #.... #.... ####. #.... #.... #####",
    'F': "##### #.... #.... ####. #.... #.... #....",
    'G': ".###. #...# #.... #.### #...# #...# .###.",
    'H': "#...# #...# #...# ##### #...# #...# #...#",
    'I': ".###. ..#.. ..#.. ..#.. ..#.. ..#.. .###.",
    'J': "..### ...#. ...#. ...#. #..#. #..#. .##..",
    'K': "#...# #..#. #.#.. ##... #.#.. #..#. #...#",
    'L': "#.... #.... #.... #.... #.... #.... #####",
    'M': "#...# ##.## #.#.# #.#.# #...# #...# #...#",
    'N': "#...# ##..# #.#.# #..## #...# #...# #...#",
    'O': ".###. #...# #...# #...# #...# #...# .###.",
    'P': "####. #...# #...# ####. #.... #.... #....",
    'Q': ".###. #...# #...# #...# #.#.# #..#. .##.#",
    'R': "####. #...# #...# ####. #.#.. #..#. #...#",
    'S': ".###. #...# #.... .###. ....# #...# .###.",
    'T': "##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..",
    'U': "#...# #...# #...# #...# #...# #...# .###.",
    'V': "#...# #...# #...# #...# #...# .#.#. ..#..",
    'W': "#...# #...# #...# #.#.# #.#.# ##.## #...#",
    'X': "#...# #...# .#.#. ..#.. .#.#. #...# #...#",
    'Y': "#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..",
    'Z': "##### ....# ...#. ..#.. .#... #.... #####",
    '0': ".###. #..## #.#.# #.#.# ##..# #...# .###.",
    '1': "..#.. .##.. ..#.. ..#.. ..#.. ..#.. .###.",
    '2': ".###. #...# ....# ...#. ..#.. .#... #####",
    '3': ".###. #...# ....# ..##. ....# #...# .###.",
    '4': "...#. ..##. .#.#. #..#. ##### ...#. ...#.",
    '5': "##### #.... ####. ....# ....# #...# .###.",
    '6': "..##. .#... #.... ####. #...# #...# .###.",
    '7': "##### ....# ...#. ..#.. .#... .#... .#...",
    '8': ".###. #...# #...# .###. #...# #...# .###.",
    '9': ".###. #...# #...# .#### ....# ...#. .##..",
    '.': "..... ..... ..... ..... ..... ..... ..#..",
    ',': "..... ..... ..... ..... ..... ..#.. .#...",
    '-': "..... ..... ..... .###. ..... ..... .....",
    ':': "..... ..#.. ..... ..... ..... ..#.. .....",
    '/': "....# ...#. ...#. ..#.. .#... .#... #....",
    "'": "..#.. ..#.. ..... ..... ..... ..... .....",
    '!': "..#.. ..#.. ..#.. ..#.. ..#.. ..... ..#..",
    '?': ".###. #...# ...#. ..#.. ..#.. ..... ..#..",
    '<': "...#. ..#.. .#... #.... .#... ..#.. ...#.",
    '>': ".#... ..#.. ...#. ....# ...#. ..#.. .#...",
    '+': "..... ..#.. ..#.. ##### ..#.. ..#.. .....",
    '#': ".#.#. ##### .#.#. .#.#. ##### .#.#. .....",
    '*': "..... #.#.# .###. ##### .###. #.#.# .....",
    '(': "...#. ..#.. .#... .#... .#... ..#.. ...#.",
    ')': ".#... ..#.. ...#. ...#. ...#. ..#.. .#...",
}
# Interface glyph order. Lower-case stand-ins are drawn specially:
#  a..e meter cell with 0..4 quarters lit, f cursor, g frame line,
#  h small dot, i bullet (selected), j left arrow, k right arrow, l title rule
CHARS = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,-:/'!?<>+#*()abcdefghijkl"


def glyph(ch):
    c = Canvas(8, 8, 0)
    if ch in G5:
        rows = G5[ch].split()
        for y, row in enumerate(rows):
            for x, v in enumerate(row):
                if v == '#':
                    c.px(x + 1, y, 3)
        # drop shadow
        sh = c.copy()
        for y in range(8):
            for x in range(8):
                if c.p[y][x] == 3:
                    for dx, dy in ((1, 1), (1, 0), (0, 1)):
                        if 0 <= x + dx < 8 and 0 <= y + dy < 8 and c.p[y + dy][x + dx] == 0:
                            sh.p[y + dy][x + dx] = 1
        return sh
    if ch in 'abcde':
        n = 'abcde'.index(ch)
        c.rect(0, 2, 8, 4, 1)
        for q in range(4):
            c.rect(q * 2, 2, 1, 4, 3 if q < n else 1)
            c.px(q * 2 + 1, 2, 0).px(q * 2 + 1, 5, 0)
        c.hline(0, 6, 8, 0)
        return c
    if ch == 'f':
        return Canvas(8, 8).stamp(0, 0, """
........
.#......
.##.....
.###....
.##,....
.#,.....
.,......
........""")
    if ch == 'g':
        return Canvas(8, 8).hline(0, 3, 8, 2)
    if ch == 'h':
        return Canvas(8, 8).rect(3, 3, 2, 2, 2)
    if ch == 'i':
        return Canvas(8, 8).rect(2, 2, 4, 4, 3).rect(3, 3, 2, 2, 2)
    if ch == 'j':
        return Canvas(8, 8).stamp(0, 0, """
........
....#...
...##...
..###...
...##...
....#...
........
........""")
    if ch == 'k':
        return Canvas(8, 8).stamp(0, 0, """
........
...#....
...##...
...###..
...##...
...#....
........
........""")
    if ch == 'l':
        return Canvas(8, 8).hline(0, 4, 8, 2).hline(0, 5, 8, 1)
    return c


def paint_font():
    cells = [glyph(ch) for ch in CHARS]
    save_gray_png(ROOT / 'assets/interface/font.png', cells, 16, 8, 8)
    (ROOT / 'assets/interface/font.json').write_text(json.dumps({
        '_comment': 'Glyph i of font.png is chars[i]. a-e meter (0-4 quarters), f cursor, '
                    'g rule, h dot, i bullet, j/k arrows, l double rule.',
        'chars': CHARS}, indent=1))
    return len(cells)


# ----------------------------------------------------------------- title
TPAL = [
    ['#08070f', '#120f24', '#1e1838', '#2e2650'],   # 0 sky
    ['#08070f', '#171329', '#2a2444', '#4a4470'],   # 1 buildings
    ['#08070f', '#171329', '#b0703a', '#f6c870'],   # 2 lit windows / train
    ['#08070f', '#120f24', '#2a8c94', '#9cf2ea'],   # 3 title lettering
    ['#08070f', '#191b2e', '#353a58', '#7d9cb4'],   # 4 street / viaduct
    ['#08070f', '#191b2e', '#8a6a48', '#f2cf86'],   # 5 lamp light
    ['#08070f', '#171329', '#8c2a48', '#f0708a'],   # 6 red beacons
    ['#08070f', '#120f24', '#4a5878', '#c4ecf2'],   # 7 subtitle / prompt
]

LOGO = {
    'V': ["##.......##", "##.......##", ".##.....##.", ".##.....##.", ".##.....##.",
          "..##...##..", "..##...##..", "..##...##..", "...##.##...", "...##.##...",
          "....###....", "....###....", ".....#....."],
    'A': [".....#.....", "....###....", "....###....", "...##.##...", "...##.##...",
          "..##...##..", "..##...##..", "..##...##..", ".#########.", ".##.....##.",
          "##.......##", "##.......##", "##.......##"],
    'C': ["...######..", ".###....###", ".##......##", "##.........", "##.........",
          "##.........", "##.........", "##.........", "##.........", ".##......##",
          ".###....###", "...######..", "..........."],
    'N': ["##.......##", "###......##", "####.....##", "##.##....##", "##..##...##",
          "##...##..##", "##....##.##", "##.....####", "##......###", "##.......##",
          "##.......##", "##.......##", "##.......##"],
    'T': ["###########", "###########", ".....#.....", ".....#.....", ".....#.....",
          ".....#.....", ".....#.....", ".....#.....", ".....#.....", ".....#.....",
          ".....#.....", ".....#.....", ".....#....."],
}
# thicken T stem and C to 2px strokes
LOGO['T'] = [r[:5] + '#' + r[5:5] + '#' + r[7:] if i >= 2 else r for i, r in enumerate(LOGO['T'])]
LOGO['T'] = ["###########", "###########"] + ["....###...." for _ in range(11)]

SMALL = {  # 4x6 proportional glyphs for the subtitle (with lower case)
    'b': ["#...", "#...", "###.", "#..#", "#..#", "###."],
    'y': ["....", "#..#", "#..#", ".###", "...#", "###."],
    'S': [".###", "#...", ".##.", "...#", "...#", "###."],
    'n': ["....", "###.", "#..#", "#..#", "#..#", "#..#"],
    't': [".#..", "###.", ".#..", ".#..", ".#..", "..##"],
    'h': ["#...", "#...", "###.", "#..#", "#..#", "#..#"],
    'C': [".###", "#...", "#...", "#...", "#...", ".###"],
    'e': ["....", ".##.", "#..#", "####", "#...", ".###"],
    'r': ["....", "#.##", "##..", "#...", "#...", "#..."],
    'i': [".#", "..", "##", ".#", ".#", ".#"],
    'c': ["....", ".###", "#...", "#...", "#...", ".###"],
    ' ': ["..", "..", "..", "..", "..", ".."],
}


def paint_title():
    W, H = 160, 144
    rnd = random.Random(1987)
    tp = [[0] * 20 for _ in range(18)]   # palette per tile
    sh = [[0] * W for _ in range(H)]      # shade per pixel

    light = set()

    def px(x, y, v, lit=False):
        if 0 <= x < W and 0 <= y < H:
            sh[y][x] = v
            if lit:
                light.add((x, y))

    def tile_pal(tx, ty, p):
        if 0 <= tx < 20 and 0 <= ty < 18:
            tp[ty][tx] = p

    # sky: banded dither, darkest at the top
    for y in range(H):
        for x in range(W):
            t = y / 96.0
            d = ((x * 7 + y * 13) % 8) / 8.0
            v = 1 if t + d * 0.35 > 0.45 else 0
            if t > 0.75 and ((x + y) % 4 == 0):
                v = 2
            sh[y][x] = v
    # scattered faint stars / distant aircraft lights
    for _ in range(18):
        x, y = rnd.randrange(W), rnd.randrange(8, 60)
        sh[y][x] = 2
    # title rows: palette 3 (lettering), subtitle rows: palette 7
    for ty in (1, 2, 3):
        for tx in range(20):
            tile_pal(tx, ty, 3)
    for ty in (5,):
        for tx in range(20):
            tile_pal(tx, ty, 7)
    # skyline (rows 6..13): buildings of varied heights
    blds = []
    x = -4
    while x < W:
        w = rnd.choice([14, 18, 22, 26, 30])
        top = rnd.choice([52, 60, 64, 70, 76, 82, 58, 48])
        blds.append((x, w, top))
        x += w + rnd.choice([0, 2, 4])
    base_y = 112
    for (bx, bw, top) in blds:
        for y in range(top, base_y):
            for xx in range(bx, bx + bw):
                if 0 <= xx < W:
                    sh[y][xx] = 1
        for xx in range(bx, bx + bw):   # roof edge highlight
            px(xx, top, 2)
        for y in range(top, base_y):     # left edge lit by the city glow
            px(bx, y, 2)
    for ty in range(6, 14):
        for tx in range(20):
            if tp[ty][tx] == 0:
                # any building pixel in this tile -> building palette
                if any(sh[ty * 8 + j][tx * 8 + i] in (1, 2) and ty * 8 + j >= 44
                       for j in range(8) for i in range(8)):
                    tile_pal(tx, ty, 1)
    # windows: lit windows only in tiles chosen to be palette 2
    for (bx, bw, top) in blds:
        for wy in range(top + 4, base_y - 4, 6):
            for wx in range(bx + 3, bx + bw - 3, 4):
                if not (0 <= wx < W):
                    continue
                tx, ty = wx // 8, wy // 8
                if ty < 6 or ty >= 14:
                    continue
                lit = rnd.random() < 0.22
                if lit and tp[ty][tx] in (1, 2):
                    tile_pal(tx, ty, 2)
                v = 3 if (lit and tp[ty][tx] == 2) else 0
                if tp[ty][tx] == 2 and not lit:
                    v = 0
                px(wx, wy, v, True)
                px(wx + 1, wy, v if v == 0 else 2, True)
                px(wx, wy + 1, v if v == 0 else 2, True)
                px(wx + 1, wy + 1, v if v == 0 else 2, True)
    # building tiles that became palette 2 keep highlights in shade 1 only
    for ty in range(18):
        for tx in range(20):
            if tp[ty][tx] == 2:
                for j in range(8):
                    for i in range(8):
                        y, x = ty * 8 + j, tx * 8 + i
                        if sh[y][x] == 2 and not (x % 4 in (0, 1) and False):
                            pass
    # red beacons on the three tallest roofs
    for (bx, bw, top) in sorted(blds, key=lambda b: b[2])[:3]:
        cx = bx + bw // 2
        if 0 <= cx < W:
            tx, ty = cx // 8, (top - 3) // 8
            tile_pal(tx, ty, 6)
            for j in range(8):
                for i in range(8):
                    y, x = ty * 8 + j, tx * 8 + i
                    if sh[y][x] == 2:
                        sh[y][x] = 1
            px(cx, top - 3, 3, True); px(cx, top - 2, 2, True); px(cx - 1, top - 3, 2, True); px(cx + 1, top - 3, 2, True)
            for y in range(top - 1, top):
                px(cx, y, 1)
    # elevated railway across rows 96..103 with a lit train
    for y in range(96, 104):
        for x in range(W):
            sh[y][x] = 1 if y not in (96, 103) else (2 if y == 96 else 0)
    for x in range(0, W, 24):
        for y in range(104, 112):
            px(x + 10, y, 1); px(x + 11, y, 1); px(x + 12, y, 0)
    for tx in range(20):
        tile_pal(tx, 12, 4)
    for tx in range(4, 13):           # the train: warm window strip
        tile_pal(tx, 12, 2)
    for x in range(34, 102):
        for y in range(97, 103):
            sh[y][x] = 1
        if (x - 34) % 23 in (21, 22):
            continue
        px(x, 99, 3 if (x // 3) % 2 == 0 else 2, True)
        px(x, 100, 2, True)
        px(x, 97, 2, True)
    # street (rows 112..143): wet asphalt with reflections
    for y in range(112, H):
        for x in range(W):
            v = 1
            if (x * 5 + y * 3) % 17 == 0:
                v = 0
            sh[y][x] = v
    for ty in range(14, 18):
        for tx in range(20):
            tile_pal(tx, ty, 4)
    for x in range(W):                 # curb line
        px(x, 112, 2)
        px(x, 113, 0)
    # reflections of lit windows: vertical dashes in the wet street
    for (bx, bw, top) in blds:
        for wx in range(bx + 3, bx + bw - 3, 4):
            if 0 <= wx < W and rnd.random() < 0.35:
                for y in range(116, 140, 3):
                    if rnd.random() < 0.6:
                        px(wx, y, 2 if rnd.random() < 0.7 else 3)
    # the streetlamp and its pool of light (palette 5)
    lx = 112
    for tx in range(11, 17):
        for ty in range(14, 18):
            tile_pal(tx, ty, 5)
    for ty in (10, 11, 13):
        tile_pal(13, ty, 4)
        tile_pal(14, ty, 4)
    for y in range(78, 134):
        px(lx, y, 2); px(lx + 1, y, 1)
    tile_pal(13, 9, 5); tile_pal(14, 9, 5)
    for x in range(lx - 8, lx + 2):
        px(x, 76, 2); px(x, 77, 1)
    px(lx - 8, 78, 3, True); px(lx - 7, 78, 3, True); px(lx - 9, 78, 2, True); px(lx - 6, 78, 2, True)
    for x in range(lx - 8, lx + 2):
        light.add((x, 76))
    for y in range(78, 134):
        light.add((lx, y))
    for y in range(116, H):
        half = min(20.0, (y - 112) * 0.7 + 5)
        for x in range(W):
            d = abs(x - (lx - 7)) / half
            if tp[y // 8][x // 8] == 5 and d < 1.0:
                if d < 0.45 and (x + y) % 2 == 0:
                    sh[y][x] = 3
                elif d < 0.8 and (x + y) % 2 == 0:
                    sh[y][x] = 2
                elif (x + y) % 3 == 0:
                    sh[y][x] = 2
                light.add((x, y))
    # a solitary figure in the light: silhouette, shade 0
    fig = ["..##..", ".####.", ".####.", "..##..", ".####.", "######", "######", "######",
           ".####.", ".####.", ".####.", ".##.#.", ".##.#.", ".#..#.", ".#..#."]
    fx, fy = lx - 14, 119
    for j, row in enumerate(fig):
        for i, v in enumerate(row):
            if v == '#':
                px(fx + i, fy + j, 0)
    # a long shadow
    for i in range(14):
        px(fx + 6 + i, fy + 15 + i // 6, 0)
    # light-palette tiles: anything bright that is not a light is demoted
    for ty in range(18):
        for tx in range(20):
            if tp[ty][tx] in (2, 5, 6):
                for j in range(8):
                    for i in range(8):
                        y, x = ty * 8 + j, tx * 8 + i
                        if sh[y][x] >= 2 and (x, y) not in light:
                            sh[y][x] = 1
    # lettering
    letters = 'VACANT'
    lw, gap = 11, 8
    total = len(letters) * lw + (len(letters) - 1) * gap
    x0 = (W - total) // 2
    y0 = 10
    for n, ch in enumerate(letters):
        for j, row in enumerate(LOGO[ch]):
            for i, v in enumerate(row):
                if v == '#':
                    px(x0 + n * (lw + gap) + i, y0 + j, 3)
    # glow: shade 2 under-shadow
    for y in range(y0, y0 + 16):
        for x in range(W):
            if sh[y][x] == 3:
                for dx, dy in ((1, 1), (0, 1)):
                    if sh[y + dy][x + dx] in (0, 1):
                        sh[y + dy][x + dx] = 2
    for x in range(x0, x0 + total):
        px(x, 28, 2 if x % 2 == 0 else 1)
    # "by Synth Centric"
    text = 'by Synth Centric'
    widths = [len(SMALL[c][0]) + 1 for c in text]
    tx0 = (W - sum(widths)) // 2
    ty0 = 41
    cx = tx0
    for ch in text:
        g = SMALL[ch]
        for j, row in enumerate(g):
            for i, v in enumerate(row):
                if v == '#':
                    px(cx + i, ty0 + j, 3)
        cx += len(g[0]) + 1
    # clear tiles of the subtitle row except text pixels to shades 0/1
    for x in range(W):
        for y in range(40, 48):
            if sh[y][x] == 2:
                sh[y][x] = 1
    # build image
    img = Image.new('RGB', (W, H))
    for y in range(H):
        for x in range(W):
            p = tp[y // 8][x // 8]
            img.putpixel((x, y), hex2rgb(TPAL[p][sh[y][x]]))
    out = ROOT / 'assets/interface'
    img.save(out / 'title.png')
    (out / 'title.json').write_text(json.dumps({
        '_comment': 'Title picture palettes and the palette used by each 8x8 tile '
                    '(18 rows of 20 digits). Pixels must use colours of their tile palette.',
        'palettes': TPAL,
        'tile_palette': [''.join(str(v) for v in row) for row in tp]}, indent=1))
    img.resize((W * 4, H * 4), Image.NEAREST).save(ROOT / 'build/preview/title_x4.png')


if __name__ == '__main__':
    (ROOT / 'build/preview').mkdir(parents=True, exist_ok=True)
    n = paint_font()
    paint_title()
    print(f'paint_interface: {n} glyphs, title picture')
