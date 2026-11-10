#!/usr/bin/env python3
"""Assemble the world overview from the generated map previews."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT = Path(__file__).resolve().parents[1]
P = ROOT / 'build/preview'
F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 15)
S = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 12)
def m(name):
    im = Image.open(P / f'map_{name}.png')
    return im.resize((im.width // 2, im.height // 2), Image.NEAREST)
# geographic layout (pixel positions of each map's top-left, 16 px per tile)
layout = {
    'pond':     (232, 0),
    'clearing': (16, 196),
    'wood':     (200, 196),
    'hill':     (8, 470),
    'village':  (200, 460),
    'creek':    (470, 520),
    'railway':  (470, 360),
    'cottage':  (20, 700),
}
labels = {'pond': 'THE POND', 'clearing': 'THE CLEARING', 'wood': 'WHISPERING WOOD', 'hill': 'THE HILL',
          'village': 'THE VILLAGE', 'creek': 'THE CREEK', 'railway': 'OLD RAILWAY PLATFORM', 'cottage': "MAE'S COTTAGE (inside)"}
W, H = 740, 870
img = Image.new('RGB', (W, H), (232, 230, 212))
d = ImageDraw.Draw(img)
for name, (x, y) in layout.items():
    im = m(name)
    img.paste(im, (x, y + 18))
    d.rectangle((x - 1, y + 17, x + im.width, y + 18 + im.height), outline=(40, 52, 44))
    d.text((x + (40 if name == 'wood' else 0), y), labels[name], fill=(30, 40, 36), font=F)
# connections
def arrow(a, b, txt=''):
    d.line((a, b), fill=(150, 90, 40), width=3)
    if txt: d.text(((a[0] + b[0]) // 2 + 4, (a[1] + b[1]) // 2 - 7), txt, fill=(150, 90, 40), font=S)
arrow((360, 196 + 18), (345, 176 + 18), 'north')
arrow((200, 196 + 18 + 128), (16 + 160, 196 + 18 + 72), 'hidden path')
arrow((328, 460 + 18), (328, 196 + 18 + 256), 'north')
arrow((200, 460 + 18 + 96), (8 + 192, 470 + 18 + 96), 'west')
arrow((456, 460 + 18 + 96), (470, 520 + 18 + 112), 'east')
arrow((470 + 184, 520 + 18), (470 + 184, 360 + 18 + 144), 'north')
arrow((260, 460 + 18 + 152), (180, 700 + 18), 'door')
d.text((330, 845), 'MOSS - world overview, drawn from the ROM\'s map data', fill=(60, 70, 64), font=S)
out = ROOT / 'docs/world_map.png'
img.save(out)
print('wrote', out, img.size)
