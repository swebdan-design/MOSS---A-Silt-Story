#!/usr/bin/env python3
"""Images for the manual: button diagram (drawn) and screenshots (from the ROM via PyBoy)."""
import sys, os, io, shutil
sys.path.insert(0, os.path.dirname(__file__))
import warnings; warnings.filterwarnings('ignore')
from PIL import Image, ImageDraw, ImageFont
OUT = 'release/manual/images'
os.makedirs(OUT, exist_ok=True)
GREEN = [(224, 240, 208), (148, 176, 128), (60, 92, 72), (16, 28, 24)]


def dmg(img):
    """Map PyBoy's grey render to a soft DMG green, then scale 3x."""
    g = img.convert('L')
    lut = sorted(set(g.getdata()))
    out = Image.new('RGB', g.size)
    def shade(v):
        return GREEN[0] if v > 200 else GREEN[1] if v > 120 else GREEN[2] if v > 50 else GREEN[3]
    out.putdata([shade(v) for v in g.getdata()])
    return out.resize((480, 432), Image.NEAREST)


def controls():
    W, H = 900, 520
    im = Image.new('RGB', (W, H), (246, 244, 232))
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 18)
    fb = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
    d.rounded_rectangle((300, 20, 600, 500), 30, fill=(200, 200, 196), outline=(90, 90, 90), width=3)
    d.rectangle((330, 50, 570, 250), fill=(90, 96, 110))
    d.rectangle((360, 70, 540, 230), fill=GREEN[1])
    d.text((450, 150), 'MOSS', fill=GREEN[3], font=fb, anchor='mm')
    # d-pad
    d.rectangle((350, 330, 420, 350), fill=(40, 40, 44)); d.rectangle((375, 305, 395, 375), fill=(40, 40, 44))
    # A B
    d.ellipse((520, 315, 556, 351), fill=(140, 30, 70)); d.ellipse((475, 340, 511, 376), fill=(140, 30, 70))
    d.text((538, 333), 'A', fill='white', font=fb, anchor='mm'); d.text((493, 358), 'B', fill='white', font=fb, anchor='mm')
    # select start
    d.rounded_rectangle((405, 420, 445, 432), 6, fill=(110, 110, 114)); d.rounded_rectangle((460, 420, 500, 432), 6, fill=(110, 110, 114))
    def label(xy, to, lines):
        d.line((xy, to), fill=(120, 80, 40), width=2)
        x, y = to
        for i, (t, bold) in enumerate(lines):
            d.text((x + (8 if x > 450 else -8), y - 10 + i * 22), t, fill=(30, 36, 34), font=fb if bold else f,
                   anchor='la' if x > 450 else 'ra')
    label((372, 340), (270, 300), [('+ pad', True), ('walk  /  move the cursor', False), ('left/right: change a setting', False)])
    label((556, 333), (640, 290), [('A', True), ('talk, look, confirm', False), ('pond: pebble or lily', False)])
    label((480, 370), (640, 380), [('B', True), ('back; hurry text', False), ('pond: change what you hold', False)])
    label((425, 432), (270, 450), [('SELECT', True), ('notebook', False), ('pond: just listen', False)])
    label((480, 432), (640, 460), [('START', True), ('menu', False), ("pond: the pond's settings", False)])
    d.text((W // 2, 505), 'Stand still to listen.', fill=(80, 70, 50), font=fb, anchor='mm')
    im.save(f'{OUT}/controls.png')


def shots():
    import play, symbols, savefile
    from pyboy import PyBoy
    rom = 'release/MOSS_by_Synth_Centric.gb'

    class P(play.Player):
        def __init__(self, sram):
            self.pb = PyBoy(rom, window='null', sound_emulated=False, ram_file=io.BytesIO(bytes(sram)), log_level='ERROR')
            self.syms = symbols.load('build/moss.noi'); self.rec = None

    def grab(p, name):
        p.pb.tick(1, True, False)
        dmg(p.pb.screen.image).save(f'{OUT}/{name}.png')

    # post-game: notebook and pause menu
    p = P(savefile.postgame())
    p.tick(300); grab(p, 'title')
    p.press('a', 3, 200); p.settle(200)
    p.press('select', 3, 40); grab(p, 'notebook_cover')
    p.press('right', 3, 60); grab(p, 'notebook_entry')
    p.press('b', 3, 40)
    p.press('start', 3, 40); grab(p, 'pause_menu')
    p.press('b', 3, 40)
    p.face('up'); p.press('a', 3, 40); p.press('a', 3, 150); p.tick(400); grab(p, 'pond_instrument')
    p.press('start', 3, 60); grab(p, 'pond_menu')
    p.press('b', 3, 60); p.press('select', 3, 200); grab(p, 'pond_listening')
    p.pb.stop(save=False)
    # listening state in the meadow on day 2
    p = P(savefile.build(day=2, postgame=0, map_=6, x=8, y=3, found=0x0F, flags=(0, 1, 2, 3, 4, 5, 33)))
    p.tick(300); p.press('a', 3, 200); p.settle(200); p.tick(150); grab(p, 'listening')
    p.pb.stop(save=False)
    for src, dst in (('03_village', 'village'), ('02_cottage_mae', 'cottage'), ('12_creek_stones', 'creek'),
                     ('17_last_bell', 'railway'), ('19_still_clearing', 'clearing'), ('29_ending_card', 'ending'),
                     ('05_pond_reveal', 'pond_reveal'), ('15_rain_village', 'rain')):
        if os.path.exists(f'build/qa/{src}.png'):
            dmg(Image.open(f'build/qa/{src}.png')).save(f'{OUT}/{dst}.png')
    shutil.copy('docs/world_map.png', f'{OUT}/world_map.png')
    Image.open('build/preview/pic_title.png').resize((480, 432), Image.NEAREST).save(f'{OUT}/title_art.png')


controls()
shots()
print('manual images:', sorted(os.listdir(OUT)))
