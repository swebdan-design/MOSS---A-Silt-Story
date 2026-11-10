#!/usr/bin/env python3
"""MOSS asset compiler (Python 3 standard library only).

Reads the editable ASCII sources in assets/ and writes C data for GBDK:
  assets/tiles/*.txt      8x8 patterns and 16x16 metatiles
  assets/sprites/*.txt    16x16 / 8x16 sprite frames
  assets/maps/*.txt       maps, legends, warps, objects, listening zones
  assets/graphics/font.txt  5x7 variable-width font
  tools/art_scenes.py     procedural full-screen pictures (title, notebook)
Outputs build/gen/{assets.h, maps.h, home_data.c, data_bankN.c} and PNG
previews in build/preview/. Fails loudly if any hardware budget is exceeded.
"""
import sys, os, re, zlib, struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'build/gen')
PREVIEW = OUT.parent / 'preview'
SHADE = {'.': 0, '-': 1, 'o': 2, '#': 3}
MAX_MAP_TILES = 144
DATA_BANKS = list(range(3, 10))
BANK_CAP = 16000

# --------------------------------------------------------------- utilities
def die(msg):
    sys.exit('gen_assets: ' + msg)

def grid(rows, w, h, where):
    if len(rows) != h or any(len(r) != w for r in rows):
        die(f'{where}: expected {w}x{h}, got {[len(r) for r in rows]} ({len(rows)} rows)')
    for r in rows:
        for c in r:
            if c not in SHADE:
                die(f'{where}: bad shade {c!r}')
    return [[SHADE[c] for c in r] for r in rows]

def tile_bytes(px):  # 8x8 list of shade rows -> 16 bytes 2bpp
    out = []
    for row in px:
        lo = hi = 0
        for x, s in enumerate(row):
            if s & 1: lo |= 0x80 >> x
            if s & 2: hi |= 0x80 >> x
        out += [lo, hi]
    return tuple(out)

def split_tiles(px):  # 16x16 -> 4 tiles TL TR BL BR (as 8x8 pixel lists)
    return [[r[x:x+8] for r in px[y:y+8]] for y in (0, 8) for x in (0, 8)]

def write_png(path, pixels, palette=((224, 240, 208), (148, 176, 128), (60, 92, 72), (16, 28, 24)), scale=1):
    h = len(pixels); w = len(pixels[0])
    raw = bytearray()
    for y in range(h * scale):
        raw.append(0)
        row = pixels[y // scale]
        for x in range(w * scale):
            raw += bytes(palette[row[x // scale] & 3])
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w*scale, h*scale, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b'')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)

def carr(name, data, ctype='uint8_t', static=False):
    body = ',\n'.join('    ' + ', '.join(f'0x{b:02X}' if ctype == 'uint8_t' else str(b) for b in data[i:i+16])
                      for i in range(0, len(data), 16)) or '    0'
    return f'{"static " if static else ""}const {ctype} {name}[] = {{\n{body}\n}};\n'

# --------------------------------------------------------------- tiles
class TileSet:
    def __init__(self, path):
        self.patterns, self.meta, self.flags, self.anims = {}, {}, {}, []
        lines = Path(path).read_text().splitlines()
        i = 0
        while i < len(lines):
            ln = lines[i].rstrip()
            if ln.startswith('-- '):
                name = ln[3:].strip()
                self.patterns[name] = grid([l.strip() for l in lines[i+1:i+9]], 8, 8, f'{path}:{name}')
                i += 9; continue
            if ln.startswith('anim '):
                self.anims.append(ln.split()[1:]); i += 1; continue
            if ln.startswith('== '):
                parts = ln[3:].split()
                name, fl = parts[0], set(parts[1:])
                body = lines[i+1].strip()
                if len(parts) >= 3 and parts[1] in ('hflip', 'vflip'):
                    body = parts[1] + ' ' + parts[2]; fl = set(parts[3:]); i -= 1
                if body.startswith(('fill ', 'quad ', 'hflip ', 'vflip ')):
                    op, *args = body.split()
                    if op == 'fill': p = self.patterns[args[0]]; px = self.join(p, p, p, p)
                    elif op == 'quad': px = self.join(*[self.patterns[a] for a in args])
                    elif op == 'hflip': px = [list(reversed(r)) for r in self.meta[args[0]]]; fl |= self.flags[args[0]]
                    else: px = list(reversed(self.meta[args[0]])); fl |= self.flags[args[0]]
                    i += 2
                else:
                    px = grid([l.strip() for l in lines[i+1:i+17]], 16, 16, f'{path}:{name}')
                    i += 17
                self.meta[name] = px; self.flags[name] = fl
                continue
            i += 1

    @staticmethod
    def join(a, b, c, d):
        return [a[y] + b[y] for y in range(8)] + [c[y] + d[y] for y in range(8)]

def coll_flags(fl):
    v = 0
    if 'solid' in fl: v |= 1
    if 'water' in fl: v |= 2
    if 'tall' in fl: v |= 4
    return v

# --------------------------------------------------------------- sprites
def load_sprites(path):
    frames, order = {}, []
    lines = Path(path).read_text().splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln.startswith('== '):
            parts = ln[3:].split()
            w = int(parts[1]) if len(parts) > 1 else 16
            frames[parts[0]] = grid([l.strip() for l in lines[i+1:i+17]], w, 16, f'{path}:{parts[0]}')
            order.append(parts[0]); i += 17
        else:
            i += 1
    tiles, index = [], {}
    for name in order:
        px = frames[name]
        index[name] = len(tiles)
        cols = (0, 8) if len(px[0]) == 16 else (0,)
        for x in cols:
            for y in (0, 8):
                tiles.append(tile_bytes([r[x:x+8] for r in px[y:y+8]]))
    if len(tiles) > 128:
        die(f'sprites use {len(tiles)} tiles; the 0x8000 sprite area holds 128')
    return tiles, index, frames, order

# --------------------------------------------------------------- font
def load_font(path):
    glyphs = {}
    lines = Path(path).read_text().splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith('== '):
            ch = lines[i][3:]
            rows = [l.strip() for l in lines[i+1:i+8]]
            glyphs[ch] = rows; i += 8
        else:
            i += 1
    table = [0xFF] * 96
    data, widths = [], []
    for ch, rows in glyphs.items():
        w = max([max([x + 1 for x, c in enumerate(r) if c == '#'] or [0]) for r in rows] or [1])
        table[ord(ch) - 32] = len(widths)
        widths.append(max(w, 1))
        for r in rows:
            b = 0
            for x, c in enumerate(r):
                if c == '#': b |= 0x80 >> x
            data.append(b)
    return table, data, widths, glyphs

# --------------------------------------------------------------- maps
class Map:
    pass

def load_legend(path):
    leg = {}
    for ln in Path(path).read_text().splitlines():
        ln = ln.split('#', 1)[0] if not ln.startswith('#=') else ln
        for tok in ln.split():
            if '=' in tok and len(tok) >= 3 and tok[1] == '=':
                leg[tok[0]] = tok[2:]
    return leg

def parse_map(path, default_legend, tilesets):
    m = Map()
    m.legend = dict(default_legend)
    m.warps, m.objs, m.listens, m.patches, m.points = [], [], [], {}, {}
    lines = Path(path).read_text().splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i].rstrip()
        tok = ln.split()
        if not tok or ln.startswith('#'):
            i += 1; continue
        if tok[0] == 'map':
            m.name = tok[1]
            kv = dict(t.split('=') for t in tok[2:])
            m.w, m.h = int(kv['w']), int(kv['h'])
            m.tiles = kv.get('tiles', 'outdoor'); m.scene = kv.get('scene', 'SILENT')
        elif tok[0] == 'legend':
            for t in tok[1:]:
                m.legend[t[0]] = t[2:]
        elif tok[0] == 'grid':
            rows = lines[i+1:i+1+m.h]
            if any(len(r) != m.w for r in rows):
                die(f'{path}: grid rows must be {m.w} wide: {[len(r) for r in rows]}')
            m.grid = rows; i += 1 + m.h; continue
        elif tok[0] == 'warp':
            x, y, dest, tx, ty, d = tok[1], tok[2], tok[3], tok[4], tok[5], tok[6]
            m.warps.append((int(x), int(y), dest, int(tx), int(ty), d))
        elif tok[0] == 'obj':
            m.objs.append((tok[1], int(tok[2]), int(tok[3])))
        elif tok[0] == 'listen':
            m.listens.append((tok[1], *map(int, tok[2:6])))
        elif tok[0] == 'patch':
            m.patches.setdefault(tok[1], []).append((int(tok[2]), int(tok[3]), tok[4]))
        elif tok[0] == 'point':
            m.points[tok[1]] = (int(tok[2]), int(tok[3]))
        i += 1
    ts = tilesets[m.tiles]
    def resolve(entry):
        name, *mods = entry.split('+')
        flags = set(ts.flags[name]) if name in ts.meta else die(f'{path}: unknown metatile {name}')
        for md in mods:
            if md.startswith('no'): flags.discard(md[2:])
            else: flags.add(md)
        return name, flags
    m.cells = []
    for y, row in enumerate(m.grid):
        for x, ch in enumerate(row):
            if ch not in m.legend: die(f'{path}: char {ch!r} at {x},{y} not in legend')
            m.cells.append(resolve(m.legend[ch]))
    m.patch_cells = {k: [(x, y, resolve(e)) for x, y, e in v] for k, v in m.patches.items()}
    return m

# --------------------------------------------------------------- main
def main():
    OUT.mkdir(parents=True, exist_ok=True)
    tilesets = {p.stem: TileSet(p) for p in sorted((ROOT / 'assets/tiles').glob('*.txt'))}
    spr_tiles, spr_index, spr_frames, spr_order = load_sprites(ROOT / 'assets/sprites/characters.txt')
    font_table, font_data, font_w, font_glyphs = load_font(ROOT / 'assets/graphics/font.txt')
    legend = load_legend(ROOT / 'assets/maps/legend.txt')
    maps = [parse_map(p, legend, tilesets) for p in sorted((ROOT / 'assets/maps').glob('*.txt')) if p.stem != 'legend']
    order = ['cottage', 'village', 'wood', 'pond', 'creek', 'railway', 'hill', 'clearing']
    maps.sort(key=lambda m: order.index(m.name) if m.name in order else 99)
    names = [m.name for m in maps]

    blobs = []  # (symbol, bytes, kind)
    home = []   # C text for bank 0
    hdr = ['/* Generated by tools/gen_assets.py - do not edit. */', '#ifndef MOSS_ASSETS_H', '#define MOSS_ASSETS_H',
           '#include <stdint.h>', '']
    mh = ['/* Generated by tools/gen_assets.py - do not edit. */', '#ifndef MOSS_MAPS_H', '#define MOSS_MAPS_H',
          '#include <stdint.h>', '']

    # ---- animations (home bank) ----
    anim_frames, anim_keys = [], {}
    for ts in tilesets.values():
        for a in ts.anims:
            frames = [tile_bytes(ts.patterns[n]) for n in a]
            while len(frames) < 4: frames = frames + frames
            anim_keys[frames[0]] = len(anim_frames)
            anim_frames.append(frames[:4])
    home.append(carr('anim_frames', [b for fr in anim_frames for t in fr for b in t]))
    hdr.append(f'#define ANIM_COUNT {len(anim_frames)}')
    hdr.append('extern const uint8_t anim_frames[]; /* [anim][4 frames][16] */')

    # ---- maps ----
    map_rows = []
    for mi, m in enumerate(maps):
        ts = tilesets[m.tiles]
        uniq, ids = [], {}
        def tid(t):
            if t not in ids:
                ids[t] = len(uniq); uniq.append(t)
            return ids[t]
        meta_ids = {}
        def meta_tiles(name):
            if name not in meta_ids:
                meta_ids[name] = [tid(tile_bytes(t)) for t in split_tiles(ts.meta[name])]
            return meta_ids[name]
        tmap = [0] * (m.w * 2 * m.h * 2)
        coll = []
        for idx, (name, fl) in enumerate(m.cells):
            x, y = idx % m.w, idx // m.w
            t = meta_tiles(name)
            for k, (dx, dy) in enumerate(((0, 0), (1, 0), (0, 1), (1, 1))):
                tmap[(y*2+dy) * m.w*2 + x*2+dx] = t[k]
            coll.append(coll_flags(fl))
        patches = []
        for pname, cells in m.patch_cells.items():
            for (x, y, (name, fl)) in cells:
                t = meta_tiles(name)
                patches.append((pname, x, y, t, coll_flags(fl)))
        if len(uniq) > MAX_MAP_TILES:
            die(f'map {m.name} needs {len(uniq)} tiles (budget {MAX_MAP_TILES})')
        anims = [(i, anim_keys[t]) for i, t in enumerate(uniq) if t in anim_keys]
        m.ntiles = len(uniq)
        sym = m.name
        blobs.append((f'ts_{sym}', [b for t in uniq for b in t]))
        blobs.append((f'mp_{sym}', tmap + coll))
        # warps: x y map tx ty dir
        dirs = {'down': 0, 'up': 1, 'left': 2, 'right': 3}
        warps = []
        for (x, y, dest, tx, ty, d) in m.warps:
            if dest not in names: die(f'{m.name}: warp to unknown map {dest}')
            warps += [x, y, names.index(dest), tx, ty, dirs[d]]
        objs = []
        for oi, (n, x, y) in enumerate(m.objs):
            objs += [x, y, oi + 1]
            mh.append(f'#define OBJ_{m.name.upper()}_{n.upper()} {oi + 1}')
        lst = []
        for li, (n, x0, y0, x1, y1) in enumerate(m.listens):
            lst += [x0, y0, x1, y1, li + 1]
            mh.append(f'#define LISTEN_{m.name.upper()}_{n.upper()} {li + 1}')
        pgroups = list(m.patch_cells.keys())
        pat = []
        for (pname, x, y, t, cf) in patches:
            pat += [pgroups.index(pname) + 1, x, y, *t, cf]
        for gi, g in enumerate(pgroups):
            mh.append(f'#define PATCH_{m.name.upper()}_{g.upper()} {gi + 1}')
        for pn, (x, y) in m.points.items():
            mh.append(f'#define PT_{m.name.upper()}_{pn.upper()}_X {x}')
            mh.append(f'#define PT_{m.name.upper()}_{pn.upper()}_Y {y}')
        an = [b for a in anims for b in a]
        blobs.append((f'ob_{sym}', warps + objs + lst + pat + an + [0]))
        m.counts = (len(warps)//6, len(objs)//3, len(lst)//5, len(pat)//8, len(anims))
        mh.append(f'#define MAP_{m.name.upper()} {mi}')
        map_rows.append(m)
        # preview
        px = [[0] * (m.w * 16) for _ in range(m.h * 16)]
        for idx, (name, fl) in enumerate(m.cells):
            x, y = idx % m.w, idx // m.w
            mp = ts.meta[name]
            for yy in range(16):
                for xx in range(16):
                    px[y*16+yy][x*16+xx] = mp[yy][xx]
        write_png(PREVIEW / f'map_{m.name}.png', px, scale=2)
        print(f'map {m.name:9s} {m.w:2d}x{m.h:<2d} tiles {len(uniq):3d}/{MAX_MAP_TILES}  warps {m.counts[0]} objs {m.counts[1]} listen {m.counts[2]} patches {m.counts[3]} anim {m.counts[4]}')
    mh.append(f'#define MAP_COUNT {len(maps)}')

    # ---- sprites ----
    blobs.append(('spr_tiles', [b for t in spr_tiles for b in t]))
    for n in spr_order:
        hdr.append(f'#define SPR_{n.upper()} {spr_index[n]}')
    hdr.append(f'#define SPR_TILE_COUNT {len(spr_tiles)}')
    sheet = []
    for n in spr_order:
        f = spr_frames[n]
        if len(f[0]) == 8: f = [r + [0]*8 for r in f]
        sheet.append(f)
    rows = [sheet[i:i+8] for i in range(0, len(sheet), 8)]
    px = []
    for r in rows:
        while len(r) < 8: r = r + [[[0]*16 for _ in range(16)]]
        for y in range(16):
            px.append([p for f in r for p in f[y]])
    write_png(PREVIEW / 'sprites.png', px, scale=3)

    # ---- interface tiles (home bank) ----
    ui = TileSet(ROOT / 'assets/graphics/ui.txt')
    if len(ui.patterns) != 16: die('ui.txt must define 16 tiles')
    home.append(carr('ui_tiles', [b for p in ui.patterns.values() for b in tile_bytes(p)]))

    # ---- font (home bank: the text renderer reads it at any time) ----
    home.append(carr('font_map', font_table))
    home.append(carr('font_bits', font_data))
    home.append(carr('font_width', font_w))
    hdr += ['extern const uint8_t font_map[];', 'extern const uint8_t font_bits[];', 'extern const uint8_t font_width[];']

    # ---- full-screen pictures ----
    sys.path.insert(0, str(ROOT / 'tools'))
    import art_scenes
    pics = art_scenes.build()
    for name, (pix, budget) in pics.items():
        w, h = len(pix[0]) // 8, len(pix) // 8
        uniq, ids, tm = [], {}, []
        for ty in range(h):
            for tx in range(w):
                t = tile_bytes([row[tx*8:tx*8+8] for row in pix[ty*8:ty*8+8]])
                if t not in ids: ids[t] = len(uniq); uniq.append(t)
                tm.append(ids[t])
        if len(uniq) > budget:
            die(f'picture {name} needs {len(uniq)} tiles (budget {budget})')
        blobs.append((f'pic_{name}', [w, h, len(uniq)] + [b for t in uniq for b in t] + tm))
        write_png(PREVIEW / f'pic_{name}.png', pix, scale=2)
        print(f'picture {name:10s} {w}x{h} tiles {len(uniq)}/{budget}')

    # ---- pack blobs into banks ----
    fill = {b: 0 for b in DATA_BANKS}
    files = {b: ['/* Generated by tools/gen_assets.py - do not edit. */', f'#pragma bank {b}', '#include <stdint.h>', ''] for b in DATA_BANKS}
    bank_of = {}
    for sym, data in sorted(blobs, key=lambda x: -len(x[1])):
        for b in DATA_BANKS:
            if fill[b] + len(data) <= BANK_CAP:
                fill[b] += len(data); bank_of[sym] = b
                files[b].append(carr(sym, data)); break
        else:
            die(f'no bank space for {sym} ({len(data)} bytes)')
    for sym, data in blobs:
        hdr.append(f'extern const uint8_t {sym}[];')
        hdr.append(f'#define BANK_{sym} {bank_of[sym]}')
    for b in DATA_BANKS:
        if fill[b]:
            (OUT / f'data_bank{b}.c').write_text('\n'.join(files[b]))
        elif (OUT / f'data_bank{b}.c').exists():
            (OUT / f'data_bank{b}.c').unlink()
    print('data banks:', {b: f for b, f in fill.items() if f})
    used = [f'$(GEN)/data_bank{b}.c' for b in DATA_BANKS if fill[b]]
    (OUT / 'files.mk').write_text('GENSRC := $(GEN)/home_data.c ' + ' '.join(used) + '\n')

    # ---- map table (home bank) ----
    scenes = {m.name: m.scene for m in maps}
    home.append('#include "maps.h"\n#include "assets.h"\n#include "mapdef.h"')
    rows = []
    for m in map_rows:
        nw, no, nl, np_, na = m.counts
        rows.append(f'    {{{m.w}, {m.h}, SCENE_{m.scene}, {m.ntiles}, BANK_ts_{m.name}, ts_{m.name}, '
                    f'BANK_mp_{m.name}, mp_{m.name}, BANK_ob_{m.name}, ob_{m.name}, {nw}, {no}, {nl}, {np_}, {na}}},')
    home.append('#include "compose.h"\nconst MapDef map_defs[] = {\n' + '\n'.join(rows) + '\n};')
    (OUT / 'home_data.c').write_text('/* Generated by tools/gen_assets.py - do not edit. */\n#include <stdint.h>\n' + '\n'.join(home))
    hdr += ['', '#endif']
    mh += ['', '#endif']
    (OUT / 'assets.h').write_text('\n'.join(hdr) + '\n')
    (OUT / 'maps.h').write_text('\n'.join(mh) + '\n')
    # tileset previews
    for nm, ts in tilesets.items():
        ms = list(ts.meta.values())
        rows = [ms[i:i+10] for i in range(0, len(ms), 10)]
        px = []
        for r in rows:
            while len(r) < 10: r = r + [[[0]*16 for _ in range(16)]]
            for y in range(16):
                px.append([p for f in r for p in f[y]])
        write_png(PREVIEW / f'tiles_{nm}.png', px, scale=3)

if __name__ == '__main__':
    main()
