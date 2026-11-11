#!/usr/bin/env python3
"""Render a generator dump (raw metatile ids from tests/host_world) with the
real metatile art and night palettes: render_map.py dump.bin W H out.png [scale]"""
import json
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/asset_generation'))
from canvas import hex2rgb, gray_to_shade  # noqa: E402

CODES = 'GBWNSPAL'


def main():
    dump, w, h, out = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
    scale = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    pals = json.loads((ROOT / 'assets/palettes/palettes.json').read_text())
    prgb = [[hex2rgb(c) for c in pals['bg'][k]] for k in pals['bg_order']]
    meta = json.loads((ROOT / 'assets/tiles/metatiles.json').read_text())['metatiles']
    sheet = Image.open(ROOT / 'assets/tiles/metatiles.png').convert('RGB')
    tiles = []
    for i, m in enumerate(meta):
        im = Image.new('RGB', (16, 16))
        ox, oy = (i % 16) * 16, (i // 16) * 16
        for y in range(16):
            for x in range(16):
                q = (y // 8) * 2 + x // 8
                s = gray_to_shade(sheet.getpixel((ox + x, oy + y)))
                im.putpixel((x, y), prgb[CODES.index(m['pals'][q])][s])
        tiles.append(im)
    data = Path(dump).read_bytes()
    img = Image.new('RGB', (w * 16, h * 16))
    for y in range(h):
        for x in range(w):
            img.paste(tiles[data[y * w + x]], (x * 16, y * 16))
    if scale != 1:
        img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
    img.save(out)


if __name__ == '__main__':
    main()
