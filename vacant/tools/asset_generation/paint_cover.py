#!/usr/bin/env python3
"""VACANT album cover: an original 250x250 pixel painting, upscaled x12.

  paint_cover.py  -> artwork/VACANT_cover_250.png   (native pixel art, source)
                     artwork/VACANT_Album_Cover.png  (3000 x 3000, nearest neighbour)

One-point perspective down an empty street: towers on both sides, an
intersection, an elevated railway crossing the far end, wet asphalt that
mirrors the windows, a single streetlamp and a small figure under it.
Minimal type: VACANT at the top, SYNTH CENTRIC at the foot.
Everything is drawn procedurally with a fixed seed; no external imagery.
"""
import random
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from canvas import ROOT, hex2rgb  # noqa: E402

N = 250
VX, VY = 125, 118
rnd = random.Random(1979)
C = {k: hex2rgb(v) for k, v in {
    'k': '#06050c', 's1': '#0e0b1e', 's2': '#171230', 's3': '#231b44', 'haze': '#3a3566',
    'b0': '#0a0816', 'b1': '#130f26', 'b2': '#1d1838', 'b3': '#2c2650', 'edge': '#453c74',
    'wa': '#a8703c', 'wA': '#f0c878', 'wc': '#3f9ea6', 'wC': '#a6eee6', 'wm': '#a8406a',
    'wd': '#0d0b1c', 'r0': '#0b0a16', 'r1': '#13131f', 'r2': '#1c1d31', 'lane': '#3c3e5c',
    'wet': '#2f4f6e', 'wetc': '#5f97b0', 'lamp': '#f2cf86', 'pool1': '#4a3a34', 'pool2': '#7a6046',
    'txt': '#a6ece6', 'txt2': '#8e86bc', 'rail': '#2a2848', 'rail2': '#3f3b66',
}.items()}

img = [[C['k']] * N for _ in range(N)]


def px(x, y, c):
    if 0 <= x < N and 0 <= y < N:
        img[y][x] = C[c] if isinstance(c, str) else c


def lerp(a, b, t):
    return a + (b - a) * t


def proj(xn, yn, z):
    """near-plane point (xn, yn) at depth z -> screen"""
    return VX + (xn - VX) / z, VY + (yn - VY) / z


def quad(p, c):
    """fill convex quad p = [(x,y)*4]"""
    ys = [q[1] for q in p]
    for y in range(int(min(ys)), int(max(ys)) + 1):
        xs = []
        for i in range(4):
            (x1, y1), (x2, y2) = p[i], p[(i + 1) % 4]
            if (y1 <= y + 0.5 < y2) or (y2 <= y + 0.5 < y1):
                xs.append(x1 + (y + 0.5 - y1) * (x2 - x1) / (y2 - y1))
        if len(xs) >= 2:
            for x in range(int(round(min(xs))), int(round(max(xs)))):
                px(x, y, c)


# ------------------------------------------------------------------ sky
for y in range(VY + 2):
    for x in range(N):
        t = y / VY
        d = ((x * 5 + y * 11) % 7) / 7
        c = 's1' if t + d * 0.18 < 0.45 else 's2' if t + d * 0.18 < 0.8 else 's3'
        if t > 0.86 and (x + y) % 3 == 0:
            c = 'haze'
        px(x, y, c)
for _ in range(14):
    px(rnd.randrange(N), rnd.randrange(30, 80), 's3')

# distant skyline at the end of the street
x = 92
while x < 160:
    w = rnd.choice([4, 5, 6, 8])
    top = rnd.randint(78, 104)
    for yy in range(top, VY + 1):
        for xx in range(x, x + w):
            px(xx, yy, 'b1')
    for xx in range(x, x + w):
        px(xx, top, 'b2')
    for _ in range(w // 2):
        if rnd.random() < 0.5:
            px(rnd.randrange(x, x + w), rnd.randrange(top + 2, VY), 'wa')
    if rnd.random() < 0.3:
        px(x + w // 2, top - 1, 'wm')
    x += w + rnd.choice([0, 1, 2])

# ----------------------------------------------------------- street plane
ROAD_L, ROAD_R = 40, 210          # near-plane x of the road edges
WALK_L, WALK_R = -10, 260         # building fronts
for y in range(VY, N):
    for x in range(N):
        px(x, y, 'r1' if (x * 3 + y * 7) % 11 else 'r0')
# sidewalks
for y in range(VY, N):
    z = (N * 1.0 - VY) / max(1, (y - VY)) if y > VY else 999
    for x in range(N):
        xn = VX + (x - VX) * z
        if xn < ROAD_L or xn > ROAD_R:
            px(x, y, 'r2' if ((int(xn) // 14 + int(z * 6)) % 2) else 'r1')
        if abs(xn - ROAD_L) < 1.5 * z or abs(xn - ROAD_R) < 1.5 * z:
            px(x, y, 'lane')
# lane dashes converging to the vanishing point
for k in range(40):
    z0, z1 = 1 + k * 0.5, 1.2 + k * 0.5
    (x0, y0), (x1, y1) = proj(VX, N + 30, z0), proj(VX, N + 30, z1)
    for y in range(int(y1), int(y0) + 1):
        px(VX, y, 'lane')
# crosswalk at the near intersection edge
for z in (1.25, 1.33, 1.41):
    pass
for i in range(9):
    xn0 = ROAD_L + 6 + i * 19
    p = [proj(xn0, N + 30, 1.22), proj(xn0 + 10, N + 30, 1.22), proj(xn0 + 10, N + 30, 1.5), proj(xn0, N + 30, 1.5)]
    quad(p, 'lane')

# --------------------------------------------------------------- towers
def tower(side, z_near, z_far, top, cols, lit=0.13):
    """facade plane on the left (side=-1) or right (side=1)"""
    xn = WALK_L if side < 0 else WALK_R
    a, b = proj(xn, top, z_near), proj(xn, top, z_far)
    c, d = proj(xn, N + 30, z_far), proj(xn, N + 30, z_near)
    quad([a, b, c, d], 'b1')
    # roof edge highlight
    steps = 60
    for i in range(steps):
        t = i / steps
        x, y = proj(xn, top, lerp(z_near, z_far, t))
        px(int(x), int(y), 'edge')
    # windows on a perspective grid
    zs = [z_near + (z_far - z_near) * k / cols for k in range(cols + 1)]
    rows = int((N + 30 - top) / 13)
    for r in range(rows):
        hy0 = top + 6 + r * 13
        hy1 = hy0 + 7
        for k in range(cols):
            za = lerp(zs[k], zs[k + 1], 0.25)
            zb = lerp(zs[k], zs[k + 1], 0.75)
            q = [proj(xn, hy0, za), proj(xn, hy0, zb), proj(xn, hy1, zb), proj(xn, hy1, za)]
            roll = rnd.random()
            col = 'wd'
            if roll < lit:
                col = 'wa'
            elif roll < lit * 1.25:
                col = 'wc'
            elif roll < lit * 1.32:
                col = 'wm'
            quad(q, col)
            if col in ('wa', 'wc') and rnd.random() < 0.5:
                (x, y) = proj(xn, hy0 + 1, za)
                px(int(x) + (1 if side < 0 else 0), int(y) + 1, 'wA' if col == 'wa' else 'wC')


# far towers first (painter's algorithm)
tower(-1, 2.9, 7.5, -140, 9, 0.2)
tower(1, 3.1, 7.5, -90, 9, 0.2)
tower(-1, 1.0, 2.2, -330, 6, 0.06)
tower(1, 1.0, 2.35, -240, 6, 0.07)

# the cross street: dark gap between near and far towers, with a glow
for side in (-1, 1):
    xn = WALK_L if side < 0 else WALK_R
    za, zb = (2.2, 2.9) if side < 0 else (2.35, 3.1)
    q = [proj(xn, -400, za), proj(xn, -400, zb), proj(xn, N + 30, zb), proj(xn, N + 30, za)]
    quad(q, 's2')
    (x, y) = proj(xn, N + 30, (za + zb) / 2)
    for k in range(10):
        px(int(x) + rnd.randint(-2, 2), int(y) - k * 2, 'haze')

# elevated railway crossing the far end, with a lit train
zr = 4.2
deck_top, deck_bot = 40, 58
y0 = int(proj(VX, deck_top, zr)[1])
y1 = int(proj(VX, deck_bot, zr)[1])
for y in range(y0, y1 + 1):
    for x in range(N):
        px(x, y, 'rail' if y not in (y0, y1) else 'rail2')
for k in range(-6, 7):
    xp, _ = proj(VX + k * 70, 0, zr)
    for y in range(y1, VY + 3):
        px(int(xp), y, 'rail')
        px(int(xp) + 1, y, 'b0')
for x in range(30, 170):
    if (x - 30) % 22 in (20, 21):
        continue
    px(x, y0 - 2, 'b2')
    px(x, y0 - 1, 'wa' if (x // 2) % 2 else 'wA')
    px(x, y0 - 3, 'b2')

# ----------------------------------------------------- wet reflections
for y in range(VY + 4, N):
    for x in range(N):
        if img[y][x] in (C['r1'], C['r0'], C['r2']):
            # mirror: look up the facade above the horizon
            my = VY - (y - VY) * 1.6
            if 0 <= my < VY:
                src = img[int(my)][x]
                if src in (C['wa'], C['wA']) and (y % 3 != 0):
                    px(x, y, 'pool1')
                elif src in (C['wc'], C['wC']) and (y % 3 != 0):
                    px(x, y, 'wet')
                elif src == C['wm'] and y % 2:
                    px(x, y, C['wm'])
for _ in range(400):
    x, y = rnd.randrange(N), rnd.randrange(VY + 6, N)
    if img[y][x] in (C['r1'], C['r0']):
        px(x, y, 'wet')

# ------------------------------------------------- streetlamp and figure
LX = 176
for y in range(150, 214):
    px(LX, y, 'rail2')
    px(LX + 1, y, 'b0')
for x in range(LX - 14, LX + 1):
    px(x, 149, 'rail2')
    px(x, 150, 'b0')
for x in range(LX - 16, LX - 10):
    px(x, 151, 'lamp')
# light cone and pool
for y in range(152, 232):
    half = (y - 152) * 0.42 + 2
    for x in range(int(LX - 13 - half), int(LX - 13 + half) + 1):
        d = abs(x - (LX - 13)) / max(1, half)
        base = img[y][x]
        if y > 205 and d < 1:
            if (x + y) % 2 == 0 and d < 0.5:
                px(x, y, 'pool2')
            elif (x + y) % 3 == 0:
                px(x, y, 'pool1')
        elif d < 0.3 and (x * 7 + y * 3) % 9 == 0 and base in (C['r1'], C['r0'], C['s2'], C['b1']):
            px(x, y, 'pool1')
# the figure: a silhouette with the faint glow of headphones
fig = ["..##..", ".####.", "#####.", ".####.", "..##..", ".####.", "######", "######",
       "######", ".####.", ".####.", ".####.", ".##.#.", ".##.#.", ".#..#.", ".#..#.", ".#..#."]
fx, fy = LX - 17, 202
for j, row in enumerate(fig):
    for i, v in enumerate(row):
        if v == '#':
            px(fx + i, fy + j, 'k')
px(fx + 1, fy + 2, 'wC')
for i in range(22):                       # long shadow across the wet street
    px(fx + 4 - i, fy + 17 + i // 7, 'k')

# ---------------------------------------------------------------- rain
for _ in range(170):
    x, y = rnd.randrange(N), rnd.randrange(0, N)
    for k in range(rnd.randint(3, 6)):
        if 0 <= y + k < N and 0 <= x - k // 3 < N:
            cur = img[y + k][x - k // 3]
            if cur in (C['k'], C['s1'], C['s2'], C['b1'], C['wd'], C['r1'], C['r0'], C['b0']):
                px(x - k // 3, y + k, 's3' if k else 'haze')

# ---------------------------------------------------------- typography
LOGO = {
    'V': ["#.......#", "#.......#", ".#.....#.", ".#.....#.", "..#...#..", "..#...#..", "...#.#...", "...#.#...", "....#...."],
    'A': ["....#....", "...#.#...", "...#.#...", "..#...#..", "..#...#..", ".#######.", ".#.....#.", "#.......#", "#.......#"],
    'C': ["..#####..", ".#.....#.", "#........", "#........", "#........", "#........", "#........", ".#.....#.", "..#####.."],
    'N': ["#.......#", "##......#", "#.#.....#", "#..#....#", "#...#...#", "#....#..#", "#.....#.#", "#......##", "#.......#"],
    'T': ["#########", "....#....", "....#....", "....#....", "....#....", "....#....", "....#....", "....#....", "....#...."],
}
word, gap = 'VACANT', 9
total = len(word) * 9 + (len(word) - 1) * gap
x0, y0 = (N - total) // 2, 20
for n, ch in enumerate(word):
    for j, row in enumerate(LOGO[ch]):
        for i, v in enumerate(row):
            if v == '#':
                px(x0 + n * (9 + gap) + i, y0 + j, 'txt')
SMALL = {
    'S': ".## #.. .#. ..# ##.", 'Y': "#.# #.# .#. .#. .#.", 'N': "##. #.# #.# #.# #.#",
    'T': "### .#. .#. .#. .#.", 'H': "#.# #.# ### #.# #.#", 'C': ".## #.. #.. #.. .##",
    'E': "### #.. ##. #.. ###", 'R': "##. #.# ##. #.# #.#", 'I': "### .#. .#. .#. ###", ' ': "... ... ... ... ...",
}
text = 'SYNTH CENTRIC'
w = len(text) * 6 - 2
x0 = (N - w) // 2
for n, ch in enumerate(text):
    g = SMALL[ch].replace(' ', '')
    for j in range(5):
        for i in range(3):
            if g[j * 3 + i] == '#':
                px(x0 + n * 6 + i, 35 + j, 'txt2')

# ---------------------------------------------------------------- write
out = Image.new('RGB', (N, N))
for y in range(N):
    for x in range(N):
        out.putpixel((x, y), img[y][x])
art = ROOT / 'artwork'
art.mkdir(exist_ok=True)
out.save(art / 'VACANT_cover_250.png')
out.resize((3000, 3000), Image.NEAREST).save(art / 'VACANT_Album_Cover.png', optimize=True)
out.resize((750, 750), Image.NEAREST).save(ROOT / 'build/preview/cover_750.png')
print('paint_cover: artwork/VACANT_Album_Cover.png (3000x3000) from a 250x250 painting')
