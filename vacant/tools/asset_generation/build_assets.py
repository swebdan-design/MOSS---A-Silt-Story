#!/usr/bin/env python3
"""VACANT asset compiler: source PNG/JSON in assets/ -> C data in build/gen/.

Reads
  assets/palettes/palettes.json
  assets/tiles/metatiles.png + metatiles.json      city metatiles
  assets/tiles/animations.png + animations.json    animated 8x8 tiles
  assets/sprites/player.png, rain.png, train.png   sprite / train frames
  assets/interface/font.png + font.json            8x8 interface font
  assets/interface/title.png + title.json          title illustration
Writes
  build/gen/assets.h        names, counts, VRAM layout
  build/gen/home_data.c     bank 0: palettes (read by the interrupt-time engine)
  build/gen/world_data.c    bank 1: metatile tables
  build/gen/gfx_data.c      bank 2: tile graphics, animation frames
  build/gen/ui_data.c       bank 3: font and title picture
  build/preview/*.png       coloured previews
Fails if any VRAM budget is exceeded.
"""
import json
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from canvas import ROOT, gray_to_shade, hex2rgb  # noqa: E402

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/gen'
PREVIEW = OUT.parent / 'preview'
PAL_CODES = {'G': 0, 'B': 1, 'W': 2, 'N': 3, 'S': 4, 'P': 5, 'A': 6, 'L': 7}

# VRAM layout (tile indices in the 0x8000 addressing mode)
BANK0_WORLD = range(0, 192)       # world BG tiles, VRAM bank 0
FONT_BASE = 192                   # 64 interface glyphs, VRAM bank 0
BANK1_SPRITES = range(0, 48)      # sprite tiles, VRAM bank 1
BANK1_WORLD = range(48, 256)      # world BG tiles, VRAM bank 1
TRAIN_TILES = 6                   # reserved at the end of bank-1 world range


def die(msg):
    sys.exit('build_assets: ' + msg)


def load_sheet(path, cell_w, cell_h):
    img = Image.open(path).convert('RGB')
    cols, rows = img.width // cell_w, img.height // cell_h
    cells = []
    for r in range(rows):
        for c in range(cols):
            px = [[gray_to_shade(img.getpixel((c * cell_w + x, r * cell_h + y)))
                   for x in range(cell_w)] for y in range(cell_h)]
            cells.append(px)
    return cells


def tile_bytes(px):
    out = []
    for row in px:
        lo = hi = 0
        for x, s in enumerate(row):
            if s & 1:
                lo |= 0x80 >> x
            if s & 2:
                hi |= 0x80 >> x
        out += [lo, hi]
    return tuple(out)


def flips(px):
    fx = [r[::-1] for r in px]
    fy = px[::-1]
    fxy = [r[::-1] for r in fy]
    return [(px, 0), (fx, 0x20), (fy, 0x40), (fxy, 0x60)]


def carr(name, data, ctype='uint8_t'):
    fmt = (lambda b: f'0x{b:02X}') if ctype == 'uint8_t' else (lambda b: f'0x{b:04X}')
    body = ',\n'.join('    ' + ', '.join(fmt(b) for b in data[i:i + 16])
                      for i in range(0, len(data), 16)) or '    0'
    return f'const {ctype} {name}[] = {{\n{body}\n}};\n'


def rgb555(h):
    r, g, b = hex2rgb(h)
    return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)


class TilePool:
    """Deduplicating tile store with flip matching, split over two VRAM banks."""

    def __init__(self):
        self.tiles = []          # list of 8x8 pixel lists
        self.lookup = {}         # bytes -> (slot, flipattr)

    def add(self, px, allow_flip=True):
        tb = tile_bytes(px)
        if tb in self.lookup:
            return self.lookup[tb]
        if allow_flip:
            for fpx, fa in flips(px)[1:]:
                fb = tile_bytes(fpx)
                if fb in self.lookup:
                    slot, a = self.lookup[fb]
                    if a == 0:
                        return slot, fa
        slot = len(self.tiles)
        self.tiles.append(px)
        self.lookup[tb] = (slot, 0)
        return slot, 0

    def vram(self, slot):
        """slot -> (vram bank, tile index)"""
        n0 = len(BANK0_WORLD)
        if slot < n0:
            return 0, BANK0_WORLD[slot]
        i = slot - n0
        if i >= len(BANK1_WORLD) - TRAIN_TILES:
            die(f'world tiles exceed VRAM budget ({len(self.tiles)} > '
                f'{n0 + len(BANK1_WORLD) - TRAIN_TILES})')
        return 1, BANK1_WORLD[i]


def split4(px):
    return [[r[x:x + 8] for r in px[y:y + 8]] for y in (0, 8) for x in (0, 8)]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    PREVIEW.mkdir(parents=True, exist_ok=True)
    pals = json.loads((ROOT / 'assets/palettes/palettes.json').read_text())
    order = pals['bg_order']
    assert [order.index(k) for k in order] == list(range(8))

    meta = json.loads((ROOT / 'assets/tiles/metatiles.json').read_text())
    mts = meta['metatiles']
    cells = load_sheet(ROOT / 'assets/tiles/metatiles.png', 16, 16)[:len(mts)]
    if len(cells) < len(mts):
        die('metatiles.png has fewer cells than metatiles.json entries')
    if len(mts) > 255:
        die('too many metatiles (max 255)')

    anims = json.loads((ROOT / 'assets/tiles/animations.json').read_text())['anims']
    arows = load_sheet(ROOT / 'assets/tiles/animations.png', 32, 8)
    pool = TilePool()
    anim_slots = []
    for a, row in zip(anims, arows):
        frames = [[r[i * 8:(i + 1) * 8] for r in row] for i in range(a['frames'])]
        slot, _ = pool.add(frames[0], allow_flip=False)
        anim_slots.append((a['tag'], slot, frames))

    mt_tiles, mt_attrs, mt_flags = [], [], []
    for m, px in zip(mts, cells):
        for q, t in enumerate(split4(px)):
            slot, fa = pool.add(t)
            bank, idx = pool.vram(slot)
            pal = PAL_CODES[m['pals'][q]]
            prio = 0x80 if m['prio'][q] == '1' else 0
            mt_tiles.append(idx)
            mt_attrs.append(pal | (bank << 3) | fa | prio)
        f = 0
        if m['solid']:
            f |= 1
        if m['blink']:
            f |= 2 | (PAL_CODES[m['blink']] << 4)
        mt_flags.append(f)
    for _, slot, _ in anim_slots:
        pool.vram(slot)
    nworld = len(pool.tiles)
    n0 = min(nworld, len(BANK0_WORLD))
    n1 = nworld - n0

    # ---- sprites (8x16 mode: each 16x16 frame = left and right 8x16 halves)
    spr_tiles = []
    sprite_frames = {}

    def add_sprite_sheet(name, path, fw, fh):
        cells = load_sheet(path, fw, fh)
        idxs = []
        for c in cells:
            if all(v == 0 for r in c for v in r) and name != 'player':
                continue
            for x in range(0, fw, 8):
                top = [r[x:x + 8] for r in c[0:8]]
                bot = [r[x:x + 8] for r in c[8:16]]
                idxs.append(len(spr_tiles))
                spr_tiles.append(top)
                spr_tiles.append(bot)
        sprite_frames[name] = idxs
    add_sprite_sheet('player', ROOT / 'assets/sprites/player.png', 16, 16)
    add_sprite_sheet('rain', ROOT / 'assets/sprites/rain.png', 8, 16)
    if len(spr_tiles) > len(BANK1_SPRITES):
        die(f'sprite tiles {len(spr_tiles)} exceed {len(BANK1_SPRITES)}')
    train = load_sheet(ROOT / 'assets/sprites/train.png', 8, 8)
    # train.png is 3 columns x 2 rows: nose, body, gap (top row, bottom row)
    train_tiles = [train[0], train[1], train[2], train[3], train[4], train[5]]
    train_base = BANK1_WORLD[-TRAIN_TILES]

    # ---- font + title (UI bank)
    font = json.loads((ROOT / 'assets/interface/font.json').read_text())
    fcells = load_sheet(ROOT / 'assets/interface/font.png', 8, 8)[:len(font['chars'])]
    if len(fcells) > 64:
        die('font exceeds 64 glyphs')
    title = json.loads((ROOT / 'assets/interface/title.json').read_text())
    timg = Image.open(ROOT / 'assets/interface/title.png').convert('RGB')
    tpals = [[hex2rgb(c) for c in p] for p in title['palettes']]
    tpal_of = title['tile_palette']        # 18 strings of 20 digits
    ttiles, tlook, tmap, tattr = [], {}, [], []
    for ty in range(18):
        for tx in range(20):
            p = int(tpal_of[ty][tx])
            cols = tpals[p]
            px = []
            for y in range(8):
                row = []
                for x in range(8):
                    c = timg.getpixel((tx * 8 + x, ty * 8 + y))
                    best = min(range(4), key=lambda i: sum((a - b) ** 2 for a, b in zip(c, cols[i])))
                    row.append(best)
                px.append(row)
            tb = tile_bytes(px)
            if tb not in tlook:
                tlook[tb] = len(ttiles)
                ttiles.append(tb)
            t = tlook[tb]
            # tiles 0..191 -> VRAM bank 0; the rest -> bank 1 from 48 (font and
            # sprite slots stay untouched so the title can show text and sprites)
            if t < 192:
                tmap.append(t)
                tattr.append(p)
            else:
                tmap.append(48 + t - 192)
                tattr.append(p | 0x08)
    if len(ttiles) > 192 + 208:
        die(f'title uses {len(ttiles)} tiles')

    # ---------------------------------------------------------------- write
    H = ['/* Generated by tools/asset_generation/build_assets.py - do not edit. */',
         '#ifndef VACANT_ASSETS_H', '#define VACANT_ASSETS_H', '#include <stdint.h>', '']
    for i, m in enumerate(mts):
        H.append(f'#define MT_{m["name"]} {i}u')
    H += ['', f'#define MT_COUNT {len(mts)}u',
          f'#define WORLD_TILES_B0 {n0}u', f'#define WORLD_TILES_B1 {n1}u',
          f'#define WORLD_B1_BASE {BANK1_WORLD[0]}u', f'#define FONT_BASE {FONT_BASE}u',
          f'#define FONT_COUNT {len(fcells)}u', f'#define SPRITE_TILES {len(spr_tiles)}u',
          f'#define TRAIN_BASE {train_base}u',
          '#define MTF_SOLID 0x01u', '#define MTF_BLINK 0x02u',
          '#define PAL_GROUND 0u', '#define PAL_BUILD 1u', '#define PAL_WINDOW 2u',
          '#define PAL_NEON 3u', '#define PAL_SIGNAL 4u', '#define PAL_NATURE 5u',
          '#define PAL_WATER 6u', '#define PAL_LAMP 7u', '']
    for k, v in sprite_frames.items():
        H.append(f'#define SPR_{k.upper()}_FIRST {v[0] if v else 0}u')
        H.append(f'#define SPR_{k.upper()}_COUNT {len(v)}u')
    H.append(f'#define ANIM_COUNT {len(anim_slots)}u')
    for i, (tag, slot, frames) in enumerate(anim_slots):
        H.append(f'#define ANIM_{tag} {i}u')
    H.append(f'#define TITLE_TILES {len(ttiles)}u')
    for k, w, t in [(s['key'], s['width'], s['text']) for s in meta['signs']]:
        H.append(f'#define SIGN_{k}_W {w}u /* {t} */')
    H.append('')
    # char map for the interface font
    H.append('/* interface font: ASCII -> glyph index (0 = space) */')
    H.append(f'#define FONT_CHARS "{font["chars"]}"')
    H += ['', 'extern const uint8_t mt_tiles[];', 'extern const uint8_t mt_attrs[];',
          'extern const uint8_t mt_flags[];', 'extern const uint8_t rmt_flags[];', 'extern const uint16_t bg_base_pal[32];',
          'extern const uint16_t district_build_pal[15];', 'extern const uint16_t spr_pal[16];',
          'extern const uint16_t ui_pal[4];',
          'extern const uint8_t world_gfx_b0[];', 'extern const uint8_t world_gfx_b1[];',
          'extern const uint8_t sprite_gfx[];', 'extern const uint8_t train_gfx[];',
          'extern const uint8_t anim_vram[];', 'extern const uint8_t anim_nframes[];',
          'extern const uint8_t anim_gfx[];', 'extern const uint8_t font_gfx[];',
          'extern const uint8_t title_gfx[];', 'extern const uint8_t title_map[];',
          'extern const uint8_t title_attr[];', 'extern const uint16_t title_pal[];',
          f'#define TITLE_PALS {len(tpals)}u',
          '', '#endif', '']
    (OUT / 'assets.h').write_text('\n'.join(H))

    home = ['/* Generated - palettes, bank 0. */', '#include "assets.h"']
    bg = [rgb555(c) for k in order for c in pals['bg'][k]]
    home.append(carr('bg_base_pal', bg, 'uint16_t'))
    db = [rgb555(c) for k in ('STREETS', 'INDUSTRIAL', 'RESIDENTIAL', 'LOWER', 'FORGOTTEN')
          for c in pals['district_build'][k]]
    home.append(carr('district_build_pal', db, 'uint16_t'))
    sp = [rgb555(c) for k in ('PLAYER', 'RAIN', 'CURSOR', 'SPARE') for c in pals['sprite'][k]]
    home.append(carr('spr_pal', sp, 'uint16_t'))
    home.append(carr('ui_pal', [rgb555(c) for c in pals['ui']], 'uint16_t'))
    (OUT / 'home_data.c').write_text('\n'.join(home))

    # bank 1 (generator) needs only collision flags; bank 5 (renderer) needs all
    wd = ['/* Generated - metatile flags for the generator. */', '#pragma bank 1',
          '#include "assets.h"', carr('mt_flags', mt_flags)]
    (OUT / 'world_data.c').write_text('\n'.join(wd))
    rd = ['/* Generated - metatile tables for the renderer. */', '#pragma bank 5',
          '#include "assets.h"', carr('mt_tiles', mt_tiles), carr('mt_attrs', mt_attrs),
          carr('rmt_flags', mt_flags)]
    (OUT / 'render_data.c').write_text('\n'.join(rd))

    gb0 = [b for t in pool.tiles[:n0] for b in tile_bytes(t)]
    gb1 = [b for t in pool.tiles[n0:] for b in tile_bytes(t)]
    sg = [b for t in spr_tiles for b in tile_bytes(t)]
    tg = [b for t in train_tiles for b in tile_bytes(t)]
    av, an, ag = [], [], []
    for tag, slot, frames in anim_slots:
        bank, idx = pool.vram(slot)
        av += [bank, idx]
        an.append(len(frames))
        for i in range(4):
            ag += list(tile_bytes(frames[i % len(frames)]))
    gd = ['/* Generated - tile graphics. */', '#pragma bank 2', '#include "assets.h"',
          carr('world_gfx_b0', gb0), carr('world_gfx_b1', gb1 or [0]), carr('sprite_gfx', sg),
          carr('train_gfx', tg), carr('anim_vram', av), carr('anim_nframes', an),
          carr('anim_gfx', ag)]
    (OUT / 'gfx_data.c').write_text('\n'.join(gd))

    fg = [b for c in fcells for b in tile_bytes(c)]
    tg2 = [b for t in ttiles for b in t]
    tp = [rgb555('#%02x%02x%02x' % c) for p in tpals for c in p]
    ud = ['/* Generated - interface font and title picture. */', '#pragma bank 3',
          '#include "assets.h"', carr('font_gfx', fg), carr('title_gfx', tg2),
          carr('title_map', tmap), carr('title_attr', tattr), carr('title_pal', tp, 'uint16_t')]
    (OUT / 'ui_data.c').write_text('\n'.join(ud))

    # ---- previews
    pal_rgb = [[hex2rgb(c) for c in pals['bg'][k]] for k in order]
    cols = 16
    rows = (len(mts) + cols - 1) // cols
    img = Image.new('RGB', (cols * 17, rows * 17), (40, 0, 40))
    for i, (m, px) in enumerate(zip(mts, cells)):
        ox, oy = (i % cols) * 17, (i // cols) * 17
        for y in range(16):
            for x in range(16):
                q = (y // 8) * 2 + (x // 8)
                img.putpixel((ox + x, oy + y), pal_rgb[PAL_CODES[m['pals'][q]]][px[y][x]])
    img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(PREVIEW / 'metatiles.png')
    print(f'build_assets: {len(mts)} metatiles, world tiles {nworld} '
          f'(bank0 {n0}/{len(BANK0_WORLD)}, bank1 {n1}/{len(BANK1_WORLD) - TRAIN_TILES}), '
          f'sprite tiles {len(spr_tiles)}, font {len(fcells)}, title tiles {len(ttiles)}')


if __name__ == '__main__':
    main()
