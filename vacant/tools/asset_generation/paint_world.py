#!/usr/bin/env python3
"""VACANT world painter.

Paints every city metatile (16x16 px, four 8x8 tiles) from code and writes the
editable source assets:

  assets/tiles/metatiles.png    4-level grayscale sheet, 16 metatiles per row
  assets/tiles/metatiles.json   name, palette per quadrant, flags, per metatile
  assets/tiles/animations.png   animation frames (one row per animated 8x8 tile)
  assets/tiles/animations.json

The PNG/JSON pair is the art source the build reads (tools/asset_generation/
build_assets.py). This painter is how the original art was made; re-running it
overwrites hand edits to the PNG, so edit one or the other, not both.

Palette codes per quadrant (TL TR BL BR):
  G GROUND  B BUILD  W WINDOW  N NEON  S SIGNAL  P NATURE  A WATER  L LAMP
Flags: solid (blocks walking), prio (BG drawn over sprites), blink (lamp/sign
whose palette the engine may switch off), anim tags are on 8x8 tiles.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from canvas import Canvas, ROOT, save_gray_png  # noqa: E402

MT = []        # metatile records
ANIMS = []     # (tag, [frames as 8x8 canvases])
NAMES = set()


def M(name, c, pals='G', solid=False, prio='', blink='', note=''):
    assert c.w == 16 and c.h == 16, name
    assert name not in NAMES, name
    NAMES.add(name)
    if len(pals) == 1:
        pals = pals * 4
    MT.append(dict(name=name, canvas=c, pals=pals, solid=solid,
                   prio=prio or '0000', blink=blink, note=note))


def quad(tl, tr=None, bl=None, br=None):
    tr = tr or tl
    bl = bl or tl
    br = br or tr
    c = Canvas()
    c.blit(tl, 0, 0).blit(tr, 8, 0).blit(bl, 0, 8).blit(br, 8, 8)
    return c


def T(art):
    c = Canvas(8, 8)
    c.stamp(0, 0, art)
    return c


def big(w, h, fill=0):
    return Canvas(w, h, fill)


def split_into(name, c, pals_fn, solid=True, blink=''):
    """Split a large canvas into metatiles name_X_Y (or name_X for one row)."""
    mw, mh = c.w // 16, c.h // 16
    for my in range(mh):
        for mx in range(mw):
            n = f'{name}{mx}' if mh == 1 else f'{name}{mx}_{my}'
            M(n, c.sub(mx * 16, my * 16, 16, 16), pals_fn(mx, my), solid, blink=blink)


# =========================================================== ground textures
def asph(seed, glint=True):
    c = Canvas(8, 8, 1)
    c.speckle(seed, 0.13, 0)
    c.speckle(seed + 1, 0.035, 2, where=(1,))
    if glint:
        c.speckle(seed + 2, 0.012, 3, where=(1,))
    return c


ASPH_A = asph(11)
ASPH_B = asph(23)
ASPH_C = asph(37)
ASPH_D = asph(41, glint=False)
# a long crack through tile B variant
ASPH_CRK = ASPH_C.copy().stamp(0, 0, """
 .
  .
  ..
    .
    .
     ..
       .
       .""")

SLAB = T("""
,+++++,.
+,,,,,,.
+,,,,,,.
+,,,.,,.
,,,,,,,.
,,,,,,,.
,,,,,,,.
........""")
SLAB2 = T("""
,++++,,.
+,,,,,,.
,,,,,,,.
,,.,,,,.
,,,..,,.
,,,,,.,.
,,,,,,,.
........""")
SLAB_CRK = T("""
,++++,,.
+,,.,,,.
,,,,.,,.
,,,,.,,.
,,,.,,,.
,,.,,,,.
,,,,,,,.
........""")
# curb: gutter shadow, lit curb face, then the slab
CURB_W = SLAB.copy()
for y in range(8):
    CURB_W.px(0, y, 0).px(1, y, 2)
CURB_N = SLAB.copy()
for x in range(8):
    CURB_N.px(x, 0, 0).px(x, 1, 2)
CURB_NW = CURB_N.copy()
for y in range(8):
    CURB_NW.px(0, y, 0).px(1, y, 2 if y else 0)
CURB_NW.px(1, 1, 3)
DRAIN = T("""
,++++,,.
+.....,.
+.+.+.,.
+.....,.
+.+.+.,.
+.....,.
,,,,,,,.
........""")
PUDDLE0 = T("""
,+++++,.
+,,,,,,.
+,....,.
,......,
,.#....,
,,....,.
,,,,,,,.
........""")
PUDDLE_F = [PUDDLE0, T("""
,+++++,.
+,,,,,,.
+,....,.
,..+...,
,.#.+..,
,,....,.
,,,,,,,.
........"""), T("""
,+++++,.
+,,,,,,.
+,.++.,.
,.+..+.,
,.#+.+.,
,,.++.,.
,,,,,,,.
........"""), T("""
,+++++,.
+,,,,,,.
+,+..+,.
,+....+,
,+#...+,
,,+..+,.
,,,,,,,.
........""")]
ANIMS.append(('PUDDLE', PUDDLE_F))

PLAZA = T("""
,,,+.,,,
,,,,.,,,
........
,+.,,,,.
,,.,,,,.
........
,,,,+.,,
,,,,,.,,""")
PLAZA2 = PLAZA.flipx()

GRAVEL = Canvas(8, 8, 1).speckle(5, 0.22, 0).speckle(6, 0.12, 2, where=(1,))
GRAVEL2 = Canvas(8, 8, 1).speckle(15, 0.22, 0).speckle(16, 0.12, 2, where=(1,)).speckle(17, 0.02, 3)
GRATING = T("""
+.+.+.+.
,.,.,.,.
+.+.+.+.
,.,.,.,.
+.+.+.+.
,.,.,.,.
+.+.+.+.
........""")
DARKFLOOR = Canvas(8, 8, 0).speckle(9, 0.30, 1).speckle(10, 0.04, 2, where=(1,))
DARKFLOOR2 = T("""
.,...,..
,..,....
...,..,.
.,......
....,...
..,...,.
,.....,.
...,....""")

GRASS = T("""
,,,,,,,,
,+,,,,.,
,,,,,,,,
,,,,+,,,
,.,,,+,,
,,,,,,,,
,,+,,,,.
,,,+,,,,""")
GRASS2 = T("""
,,,,,.,,
,,,+,,,,
,+,,,,,,
,,,,,,+,
,,,,,,,,
,.,,+,,,
,,,,,+,,
+,,,,,,,""")
GRASS_TALL = T("""
,+,,,+,,
,#+,,#+,
,,,,,,,,
,,,+,,,,
,.,#+,,,
,,,,,,+,
,+,,,,#+
,#+,,,,,""")
WEEDCON = T("""
,,,,,,,.
,,,,,,,+
,,,,,,+#
,,,,,,,.
,,,,,,,.
,,,,,,,.
,,,,,,,.
.+..#+..""")
WEEDCON2 = T("""
,,,,,,,.
,,,,,,,.
,,,,,,,.
,,,,,,,.
,,,,,,,.
,,,,,,,.
,,,,,,,+
...+#...""")

# lane markings (shade 2 = faded paint)
DASH_V0 = ASPH_A.copy().vline(7, 0, 8, 2)
DASH_V1 = ASPH_B.copy().vline(7, 0, 2, 2)
DASH_H0 = ASPH_A.copy().hline(0, 7, 8, 2)
DASH_H1 = ASPH_B.copy().hline(0, 7, 2, 2)
ZEB_V = ASPH_D.copy()
for x in (1, 2, 5, 6):
    ZEB_V.vline(x, 0, 8, 2)
ZEB_V.speckle(3, 0.08, 1, where=(2,))
ZEB_H = Canvas(8, 8)
for y in range(8):
    for x in range(8):
        ZEB_H.p[y][x] = ZEB_V.p[x][y]

# water: periodic 8x8 waves, 4 frames moving right
WATER_BASE = T("""
,,,,,,,,
,+++,,,,
,,,,,,,,
,,,,,+#+
,,,,,,,,
,,,,,,,,
+,,,,,++
,,,,,,,,""")
WATER_F = [WATER_BASE.shift(2 * i, 0) for i in range(4)]
ANIMS.append(('WATER', WATER_F))
WATER2 = WATER_BASE.flipx().shift(3, 0)
WATER_LIP = T("""
........
++++++++
########
++++++++
........
,,,,,,,,
,,,,,+#,
,,,,,,,,""")
SHALLOW = T("""
,,,,,,,.
,+++,,,.
,,,,,,,.
,,,,,+#.
,,,,,,,.
,,,,,,,.
,,,,,,,.
........""")


def ground(tile_a, tile_b=None):
    return quad(tile_a, tile_b or tile_a, tile_b or tile_a, tile_a)


# =================================================================== ROADS
M('ROAD', quad(ASPH_A, ASPH_B, ASPH_B, ASPH_A))
M('ROAD2', quad(ASPH_B, ASPH_C, ASPH_A, ASPH_B))
M('ROAD_CRACK', quad(ASPH_A, ASPH_CRK, ASPH_B, ASPH_A))
M('ROAD_VL', quad(ASPH_A, DASH_V0, ASPH_B, DASH_V1))
M('ROAD_VR', quad(ASPH_A, DASH_V0, ASPH_B, DASH_V1).flipx())
M('ROAD_HT', quad(ASPH_A, ASPH_B, DASH_H0, DASH_H1))
M('ROAD_HB', quad(ASPH_A, ASPH_B, DASH_H0, DASH_H1).flipy())
M('XWALK_V', quad(ZEB_V))
M('XWALK_H', quad(ZEB_H))
mh = quad(ASPH_A, ASPH_B, ASPH_B, ASPH_A).stamp(3, 3, """
  ++++++
 +,,,,,,.
+,.,.,.,,.
+,,,,,,,,.
+,.,.,.,,.
+,,,,,,,,.
+,.,.,.,,.
 ,,,,,,,.
  ......""")
M('ROAD_MANHOLE', mh)
# weedy cracked road (forgotten places) - walkable
wr = quad(ASPH_A, ASPH_CRK, ASPH_B, ASPH_A)
wr.stamp(2, 9, "+\n#+\n+").stamp(11, 2, " +\n+#\n +")
M('ROAD_WEEDS', wr)
# dark asphalt (lower city)
M('ROAD_DARK', quad(DARKFLOOR, DARKFLOOR2, DARKFLOOR2, DARKFLOOR))
# industrial road with an embedded rail pair (walkable)
rail = quad(ASPH_A, ASPH_B, ASPH_B, ASPH_A)
for y in (4, 11):
    rail.hline(0, y, 16, 0).hline(0, y + 1, 16, 2)
    for x in range(0, 16, 4):
        rail.px(x + 1, y + 1, 3)
M('ROAD_RAIL', rail)

# ================================================================ SIDEWALKS
M('WALK', quad(SLAB, SLAB, SLAB, SLAB2))
M('WALK2', quad(SLAB2, SLAB, SLAB_CRK, SLAB))
M('WALK_DRAIN', quad(SLAB, DRAIN, SLAB, SLAB))
M('WALK_PUDDLE', quad(SLAB, SLAB2, PUDDLE0, SLAB))
M('CURB_W', quad(CURB_W, SLAB, CURB_W, SLAB2))
M('CURB_E', quad(CURB_W, SLAB, CURB_W, SLAB2).flipx())
M('CURB_N', quad(CURB_N, CURB_N, SLAB, SLAB2))
M('CURB_S', quad(CURB_N, CURB_N, SLAB, SLAB2).flipy())
M('CURB_NW', quad(CURB_NW, CURB_N, CURB_W, SLAB))
M('CURB_NE', quad(CURB_NW, CURB_N, CURB_W, SLAB).flipx())
M('CURB_SW', quad(CURB_NW, CURB_N, CURB_W, SLAB).flipy())
M('CURB_SE', quad(CURB_NW, CURB_N, CURB_W, SLAB).flipx().flipy())
# lamp-lit paving: the same slabs in the LAMP palette (warm bevels)
M('GLOW', quad(SLAB, SLAB, SLAB, SLAB2), 'L')
M('GLOW_W', quad(CURB_W, SLAB, CURB_W, SLAB2), 'L')
M('GLOW_E', quad(CURB_W, SLAB, CURB_W, SLAB2).flipx(), 'L')
M('PLAZA', quad(PLAZA, PLAZA2, PLAZA2, PLAZA))
M('GRASS', quad(GRASS, GRASS2, GRASS2, GRASS), 'P')
M('GRASS_TALL', quad(GRASS_TALL, GRASS, GRASS2, GRASS_TALL), 'P')
M('WEEDS', quad(WEEDCON, SLAB, SLAB2, WEEDCON2), 'P')
M('GRAVEL', quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL))
M('GRATING', quad(GRATING))
M('DARKFLOOR', quad(DARKFLOOR, DARKFLOOR2, DARKFLOOR2, DARKFLOOR))
M('SHALLOW', quad(SHALLOW, SHALLOW.flipx(), SHALLOW.flipx(), SHALLOW), 'A')
gr = quad(SLAB, SLAB2, SLAB2, SLAB).stamp(0, 0, """
................
.++++++++++++++.
.+,.,.,.,.,.,.+.
.+............+.
.+,.,.,.,.,.,.+.
.+............+.
.+,.,.,.,.,.,.+.
.+............+.
.+,.,.,.,.,.,.+.
.+............+.
.+,.,.,.,.,.,.+.
.++++++++++++++.
................""")
M('GRATE', gr)

# ============================================================ WATER & RAIL
M('WATER', quad(WATER_F[0], WATER2, WATER2, WATER_F[0]), 'A', solid=True)
M('WATER_N', quad(WATER_LIP, WATER_LIP, WATER2, WATER_F[0]), 'A', solid=True)
M('WATER_S', quad(WATER_F[0], WATER2, WATER_LIP.flipy(), WATER_LIP.flipy()), 'A', solid=True)
lipw = Canvas(8, 8)
for y in range(8):
    for x in range(8):
        lipw.p[y][x] = WATER_LIP.p[x][y]
M('WATER_W', quad(lipw, WATER_F[0], lipw, WATER2), 'A', solid=True)
M('WATER_E', quad(lipw, WATER_F[0], lipw, WATER2).flipx(), 'A', solid=True)
cul = Canvas(16, 16, 1).stamp(0, 0, """
................
++++++++++++++++
################
++++++++++++++++
+,,,,......,,,,+
+,,..........,,+
+,............,+
+,............,+
+..............+
+..............+
+..............+
+...,,,,,,,,...+
+..,++++++++,..+
+,,,,,,,,,,,,,,+
++++++++++++++++
................""")
M('CULVERT', cul, 'A', solid=True)

# elevated railway deck: two metatile rows, one track centred on their seam
deck = big(16, 32, 1)
deck.hline(0, 0, 16, 0).hline(0, 1, 16, 2).hline(0, 2, 16, 3).hline(0, 3, 16, 2).hline(0, 4, 16, 0)
deck.speckle(44, 0.08, 0, where=(1,))
for y0 in (11, 19):
    deck.hline(0, y0, 16, 0).hline(0, y0 + 1, 16, 3).hline(0, y0 + 2, 16, 0)
for x in range(0, 16, 4):
    deck.vline(x, 9, 15, 0)
    deck.px(x, 12, 3).px(x, 20, 3)
deck.hline(0, 27, 16, 0).hline(0, 28, 16, 2).hline(0, 29, 16, 3).hline(0, 30, 16, 2).hline(0, 31, 16, 0)
M('VIA_T', deck.sub(0, 0, 16, 16), 'G', solid=True)
M('VIA_B', deck.sub(0, 16, 16, 16), 'G', solid=True)
# the same deck crossing over an intersection: walkable, drawn above sprites
M('VIA_T_OVER', deck.sub(0, 0, 16, 16), 'G', solid=False, prio='1111')
M('VIA_B_OVER', deck.sub(0, 16, 16, 16), 'G', solid=False, prio='1111')

# a passing train, painted as BG tiles the engine scrolls across the deck rows
TRAIN = {}
car = big(8, 16, 0)
TRAIN['body'] = T("""
........
++++++++
,,,,,,,,
,+,,,+,,
,,,,,,,,
,,,,,,,,
,,,,,,,,
########"""), T("""
++++++++
,,,,,,,,
,,,,,,,,
,+,,,+,,
,,,,,,,,
++++++++
........
........""")
TRAIN['nose'] = T("""
........
...+++++
..+,,,,,
.+,,,,,,
.+,,,,,,
.+,,,,,,
.+,,,,,,
.#######"""), T("""
.+++++++
.+,,,,,,
.+,,,,,,
.+,,,,,,
..+,,,,,
...+++++
........
........""")
TRAIN['gap'] = T("""
........
........
........
........
........
...++...
...++...
...++..."""), T("""
...++...
...++...
........
........
........
........
........
........""")

# =============================================================== OBJECTS
def on_slab():
    return quad(SLAB, SLAB2, SLAB2, SLAB)


lamp = on_slab().stamp(0, 0, """

   .........
  .+######+.
   ..,,,,..
      .+.
      .+.
      .+.
      .+.
      .+.
      .+.
      .+.
      .+.
     .,+,.
    ..,+,..
     .....
                """)
M('LAMP', lamp, 'L', solid=True)
M('LAMP_FLICKER', lamp, 'L', solid=True, blink='G')
lamp_off = lamp.copy().replace(3, 1, (2, 1, 9, 2))
M('LAMP_OFF', lamp_off, 'G', solid=True)

traffic = on_slab().stamp(0, 0, """

     .....
     .,#,.
     .,.,.
     .,.,.
     .....
      .+.
      .+.
      .+.
      .+.
      .+.
      .+.
     .,+,.
    ..,+,..
     .....
                """)
M('TRAFFIC', traffic, 'S', solid=True, blink='G')

vend = on_slab().stamp(0, 0, """



................
.++++++++++++++.
.+,,,,,,,,,,,,+.
.,,,,,,,,,,,,,,.
................
.##+##+##+#,,,,.
.++.++.++.+,..,.
.##+##+##+#,,,,.
.++.++.++.+,..,.
.,,,,,,,,,,,+#,.
.,......,,,,,,,.
.,,,,,,,,,,,,,,.
................""")
M('VEND_C', vend, 'GGNN', solid=True)
M('VEND_R', vend, 'GGSS', solid=True)

pole = on_slab().stamp(0, 0, """

 ..............
 .#+..++..+#.
 ..............
      .++.
      .++.
     .,++,.
     .,++,.
     ..++..
      .++.
      .++.
      .++.
     ,.++.,
    ...++...
     ......
                """)
M('POLE', pole, 'G', solid=True)

bench = on_slab().stamp(0, 0, """




 ..............
 .############.
 .,,,,,,,,,,,,.
 .############.
 .,,,,,,,,,,,,.
 .++++++++++++.
 ..,........,..
  .,.      .,.
  ...      ...


                """)
M('BENCH', bench, 'G', solid=True)

binx = on_slab().stamp(0, 0, """



     ......
    .######.
    .+,,,,+.
    .+,..,+.
    .######.
    .,+,+,,.
    .,+,+,,.
    .,+,+,,.
    .,+,+,,.
    ........
     ......

                """)
M('BIN', binx, 'G', solid=True)

hyd = on_slab().stamp(0, 0, """




      ....
     .####.
     .+##+.
    ..####..
    .#+##+#.
    ..####..
     .+##+.
     .####.
    ........
     ......

                """)
M('HYDRANT', hyd, 'S', solid=True)

bol = on_slab().stamp(0, 0, """



   ...    ...
  .###.  .###.
  .+++.  .+++.
  .,,,.  .,,,.
  .###.  .###.
  .,,,.  .,,,.
  .,,,.  .,,,.
  .....  .....
   ...    ...



                """)
M('BOLLARDS', bol, 'S', solid=True)

# trees: canopy covers the metatile; corners are shadow, so any ground works
tree_a = Canvas(16, 16, 0).stamp(0, 0, """
.....,,,,,,.....
...,,++++++,,...
..,+++#++++++,..
.,++##++++,+++,.
.,+##+++,+++++,.
,+++++,+++++,++,
,++++++++,++++,,
,+,+++++++++,++,
,++++,+++++++++,
,+++++++,++,++,,
,,+++,++++++++,.
.,,++++++,+++,,.
.,,,+++,+++,,,..
..,,,,,,,,,,,...
....,,,,,,,.....
................""")
tree_b = tree_a.copy()
tree_b.replace(3, 2)
tree_b.stamp(0, 0, """


       ##
      #  #
    ##
     #       ##


                """)
M('TREE', tree_a, 'P', solid=True)
M('TREE2', tree_b, 'P', solid=True)
# sway frames for the two top quadrants of TREE
ANIMS.append(('TREE_TL', [tree_a.sub(0, 0, 8, 8), tree_a.shift(1, 0).sub(0, 0, 8, 8)]))
ANIMS.append(('TREE_TR', [tree_a.sub(8, 0, 8, 8), tree_a.shift(1, 0).sub(8, 0, 8, 8)]))

bush = quad(GRASS, GRASS2, GRASS2, GRASS).stamp(0, 0, """


    ,,,,,,,
  ,,+++++++,,
 ,++#++++,+++,
 ,+##+,+++++++,
 ,++++++++,+++,
 ,+++,+++++++,,
 ,,+++++,++++,
  .,,+++++++,.
   ..,,,,,,..
    ........



                """)
M('BUSH', bush, 'P', solid=True)

planter = Canvas(16, 16, 1).stamp(0, 0, """
.,,,,,,,,,,,,,,.
,++#+,+++#++,++,
,+##+++,++++++,,
,+++,+++++,++++,
,,++++#+++++,++,
,+++,+##++,+++,,
,,+++++++++++,,.
.,,,,,,,,,,,,,,.
.##############.
.++++++++++++++.
.,,,,,,,,,,,,,,.
.,,,,,,,,,,,,,,.
.,+,,,,,,,,,,+,.
.,,,,,,,,,,,,,,.
................
 .............. """)
M('PLANTER', planter, 'PPGG', solid=True)

crates = Canvas(16, 16, 1).blit(quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL), 0, 0).stamp(0, 0, """

 ......
 .####.......
 .+,,+.#####.
 .+,,+.+,,,+.
 .####.+,,,+.
 .,,,,.+,,,+.
 ......#####.
  .....,,,,,..
  .####......
  .+,,+.####.
  .+,,+.+,,+.
  .####.####.
  .,,,,.,,,,.
  ...........
                """)
M('CRATES', crates, 'G', solid=True)
barrels = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL).stamp(0, 0, """

  ....   ....
 .####. .####.
 #+,,+# #+,,+#
 #,..,# #,..,#
 #+,,+# #+,,+#
 .####. .####.
  ....   ....
     ....
    .####.
    #+,,+#
    #,..,#
    #+,,+#
    .####.
     ....
                """)
M('BARRELS', barrels, 'G', solid=True)
cont = Canvas(32, 16, 0)
cont.rect(1, 1, 30, 14, 1)
for x in range(2, 30, 2):
    cont.vline(x, 2, 12, 2)
cont.hline(1, 1, 30, 3).hline(1, 14, 30, 0)
cont.vline(1, 1, 14, 0).vline(30, 1, 14, 0)
cont.rect(2, 2, 2, 12, 1).rect(28, 2, 2, 12, 1)
for p in 'GSN':
    split_into('CONT_' + p, cont, lambda mx, my, p=p: p, True)

pipe_h = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL).stamp(0, 0, """



................
################
++++++++++++++++
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................
 ,,.      .,,.
 ...      ....


                """)
M('PIPE_H', pipe_h, 'G', solid=True)
pipe_v = Canvas(16, 16)
src = pipe_h
for y in range(16):
    for x in range(16):
        pipe_v.p[y][x] = src.p[x][y]
M('PIPE_V', pipe_v, 'G', solid=True)

ventbox = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL).stamp(0, 0, """

 ..............
 .############.
 .+..........+.
 .+.,+,+,+,+.+.
 .+.+,+,+,+,.+.
 .+.,+,+,+,+.+.
 .+.+,+,+,+,.+.
 .+..........+.
 .++++++++++++.
 .,,,,,,,,,,,,.
 .,..,,..,,..,.
 .,,,,,,,,,,,,.
 ..............

                """)
M('VENTBOX', ventbox, 'G', solid=True)

trans = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL).stamp(0, 0, """
           ...
 .#..#..#. .#.
 .+..+..+. ...
..,..,..,....
.############.
.+,+,+,+,+,+,.
.+,+,+,+,+,+,.
.############.
.,,,,,,,,,,,,.
.,+,+,+,+,+,,.
.,+,+,+,+,+,,.
.,+,+,+,+,+,,.
.,,,,,,,,,,,,.
..............
 ............
                """)
M('TRANSFORMER', trans, 'GSGG', solid=True, blink='G')


def fence_tile(horizontal):
    c = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL)
    if horizontal:
        c.hline(0, 5, 16, 3)
        for y in range(6, 11):
            for x in range(16):
                c.px(x, y, 2 if (x + y) % 4 == 0 or (x - y) % 4 == 0 else 0)
        c.hline(0, 11, 16, 2)
        c.vline(0, 4, 9, 3).vline(15, 4, 9, 3)
    else:
        for y in range(16):
            for x in range(6, 10):
                c.px(x, y, 2 if (x + y) % 4 == 0 or (x - y) % 4 == 0 else 0)
            c.px(5, y, 3).px(10, y, 2)
    return c


fh = fence_tile(True)
fv = fence_tile(False)
M('FENCE_H', fh, 'G', solid=True)
M('FENCE_V', fv, 'G', solid=True)
fc = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL)
fc.blit(fh.sub(8, 0, 8, 16), 8, 0).blit(fv.sub(0, 8, 16, 8), 0, 8)
fc.rect(5, 4, 6, 7, 3).rect(6, 5, 4, 5, 2)
M('FENCE_NW', fc, 'G', solid=True)
M('FENCE_NE', fc.flipx(), 'G', solid=True)
fc2 = quad(GRAVEL, GRAVEL2, GRAVEL2, GRAVEL)
fc2.blit(fh.sub(8, 0, 8, 16), 8, 0).blit(fv.sub(0, 0, 16, 8), 0, 0)
fc2.rect(5, 4, 6, 7, 3).rect(6, 5, 4, 5, 2)
M('FENCE_SW', fc2, 'G', solid=True)
M('FENCE_SE', fc2.flipx(), 'G', solid=True)

stairs = Canvas(16, 16, 0).stamp(0, 0, """
################
+..............+
+.,,,,,,,,,,,,.+
+..............+
+.,,,,,,,,,,,,.+
+..............+
+.............,+
+..,,,,,,,,,,..+
+..............+
+..............+
+...,,,,,,,,...+
+..............+
+..............+
+..............+
+..............+
++............++""")
M('STAIRS_DOWN', stairs, 'G', solid=True)

fountain = Canvas(16, 16, 1).blit(quad(WEEDCON, SLAB, SLAB2, WEEDCON2), 0, 0).stamp(0, 0, """

    ........
  ..########..
 .##++++++++##.
 .#+........+#.
.#+..,,,,,,..+#.
.#+.,,,..,,,.+#.
.#+.,,.++.,,.+#.
.#+.,,.++.,,.+#.
.#+.,,,..,,,.+#.
.#+..,,,,,,..+#.
 .#+........+#.
 .##++++++++##.
  ..########..
    ........
                """)
M('FOUNTAIN', fountain, 'P', solid=True)

# rail track at ground level (abandoned platforms) - solid
trk = Canvas(16, 16, 0).speckle(71, 0.25, 1)
for x in range(1, 16, 4):
    trk.vline(x, 1, 14, 1).vline(x + 1, 1, 14, 1)
for y in (4, 11):
    trk.hline(0, y, 16, 3).hline(0, y + 1, 16, 2)
M('TRACK', trk, 'G', solid=True)
pedge = quad(SLAB, SLAB2, SLAB2, SLAB)
pedge.hline(0, 14, 16, 0).hline(0, 15, 16, 0)
for x in range(0, 16, 2):
    pedge.px(x, 12, 3).px(x + 1, 13, 3)
M('PLATFORM_EDGE', pedge, 'L')

# ===================================================== ROOFS (BUILD palette)
def roof_set(prefix, fill_a, fill_b, edge_rows, pals='B'):
    """9-slice roof. edge_rows = shades from the outside in (N/W/E edges);
    the south (front) parapet is drawn brighter."""
    e = edge_rows
    def paint(n, s, w, ea):
        c = quad(fill_a, fill_b, fill_b, fill_a)
        if n:
            for i, v in enumerate(e):
                c.hline(0, i, 16, v)
        if w:
            for i, v in enumerate(e):
                c.vline(i, 0 if not n else i, 16, v)
        if ea:
            for i, v in enumerate(e):
                c.vline(15 - i, 0 if not n else i, 16, v)
        if s:
            front = [0, 3, 2, 2, 0]
            for i, v in enumerate(front):
                c.hline(0, 11 + i, 16, v)
            if w:
                for i, v in enumerate(e):
                    c.vline(i, 0 if not n else i, 11 + (1 if i else 0), v)
            if ea:
                for i, v in enumerate(e):
                    c.vline(15 - i, 0 if not n else i, 11 + (1 if i else 0), v)
        return c
    for name, n, s, w, ea in [('NW', 1, 0, 1, 0), ('N', 1, 0, 0, 0), ('NE', 1, 0, 0, 1),
                              ('W', 0, 0, 1, 0), ('C', 0, 0, 0, 0), ('E', 0, 0, 0, 1),
                              ('SW', 0, 1, 1, 0), ('S', 0, 1, 0, 0), ('SE', 0, 1, 0, 1),
                              ('1W', 1, 1, 1, 0), ('1C', 1, 1, 0, 0), ('1E', 1, 1, 0, 1)]:
        M(f'{prefix}_{name}', paint(n, s, w, ea), pals, solid=True)


ROOF_A1 = Canvas(8, 8, 1).speckle(81, 0.10, 0).speckle(82, 0.05, 2, where=(1,))
ROOF_A2 = Canvas(8, 8, 1).speckle(83, 0.10, 0).speckle(84, 0.05, 2, where=(1,))
roof_set('RA', ROOF_A1, ROOF_A2, [0, 2, 2, 0])
CORR = T("""
,+,.,+,.
,+,.,+,.
,+,.,+,.
,+,.,+,.
,+,.,+,.
,+,.,+,.
,+,.,+,.
,+,.,+,.""")
CORR2 = CORR.copy().px(2, 3, 0).px(6, 6, 2)
roof_set('RB', CORR, CORR2, [0, 3, 2, 0])
CONC1 = Canvas(8, 8, 1).speckle(91, 0.16, 0).speckle(92, 0.03, 2, where=(1,)).stamp(0, 0, """

   .
    ..
      .
        """)
CONC2 = Canvas(8, 8, 1).speckle(93, 0.16, 0).speckle(94, 0.03, 2, where=(1,))
roof_set('RC', CONC1, CONC2, [0, 2, 1, 2, 0])
MOSS1 = ROOF_A1.copy().speckle(95, 0.18, 2).speckle(96, 0.05, 3)
MOSS2 = ROOF_A2.copy().speckle(97, 0.12, 2).speckle(98, 0.04, 3)
M('RD_C', quad(MOSS1, MOSS2, MOSS2, MOSS1), 'P', solid=True)

# sawtooth industrial roof band: lit glazing strip across the lower half
saw = quad(CORR, CORR2, CORR, CORR2)
saw.hline(0, 7, 16, 0)
for y in range(8, 13):
    for x in range(16):
        saw.px(x, y, 3 if (x + y) % 5 == 0 else 2)
saw.hline(0, 13, 16, 0).hline(0, 14, 16, 1)
M('RB_SAW', saw, 'BBWW', solid=True)

# rooftop details (drawn on roof A gravel)
def on_roof(art, fill=None):
    c = quad(ROOF_A1, ROOF_A2, ROOF_A2, ROOF_A1) if fill is None else fill
    return c.stamp(0, 0, art)


ac = on_roof("""

 ..............
 .++++++++++++.
 .+,,,,..,,,,+.
 .+,,,.,,.,,,+.
 .+,,.,..,.,,+.
 .+,,.,..,.,,+.
 .+,,,.,,.,,,+.
 .+,,,,..,,,,+.
 .+,,,,,,,,,,+.
 .############.
 .,,,,,,,,,,,,.
 .,,,,,,,,,,,,.
 ..............

                """)
fan_frames = []
for i, art in enumerate(["""
 ......
.,,.,,.
.,..,,.
..,..,.
.,,..,.
.,,.,,.
 ......""", """
 ......
.,.,,,.
.,..,..
.,,..,,
..,..,.
.,,,.,.
 ......""", """
 ......
.,,,.,.
..,.,,.
.,,..,.
.,.,,..
.,.,,,.
 ......""", """
 ......
.,,.,,.
.,,..,.
.,...,.
.,..,,.
.,,.,,.
 ......"""]):
    f = ac.copy().stamp(3, 3, art)
    fan_frames.append(f)
ac = fan_frames[0]
M('ROOF_AC', ac, 'B', solid=True)
# the fan straddles all four 8x8 tiles; animate the two upper ones
ANIMS.append(('FAN_TL', [f.sub(0, 0, 8, 8) for f in fan_frames]))
ANIMS.append(('FAN_TR', [f.sub(8, 0, 8, 8) for f in fan_frames]))
ANIMS.append(('FAN_BL', [f.sub(0, 8, 8, 8) for f in fan_frames]))
ANIMS.append(('FAN_BR', [f.sub(8, 8, 8, 8) for f in fan_frames]))

tank = on_roof("""
    ........
  ..########..
 .#++++++++++#.
 .#+,,,,,,,,+#.
 .#+,,,,,,,,+#.
 .#+,,,,,,,,+#.
 ..##########..
 .,+,,,,,,,,+,.
 .,+,,,,,,,,+,.
 .,+,,,,,,,,+,.
 .,+,,,,,,,,+,.
 ..##########..
  .+.      .+.
  .+.      .+.
  ...      ...
                """)
M('ROOF_TANK', tank, 'B', solid=True)
ant = on_roof("""
       ..
      .#.
      .+.
   ....+....
   .#++++++#.
      .+.
    ...+...
    .++++++.
      .+.
      .+.
      .+.
     .,+,.
    .,,+,,.
    .......

                """)
M('ROOF_ANTENNA', ant, 'B', solid=True)
sky_fill = quad(Canvas(8, 8, 1).speckle(85, 0.1, 0), Canvas(8, 8, 1).speckle(86, 0.1, 0))
sky = sky_fill.stamp(0, 0, """

 ..............
 .############.
 .#++++.+++++#.
 .#+,,+.+,,,+#.
 .#+,,+.+,,,+#.
 .#++++.+++++#.
 ..............
 .#+++.++++++#.
 .#+,,.+,,,,+#.
 .#++++.+++++#.
 .############.
 ..............


                """)
M('ROOF_SKYLIGHT', sky, 'W', solid=True)
hatch = on_roof("""


   ..........
   .########.
   .+,,,,,,+.
   .+,++++,+.
   .+,,,,,,+.
   .+,++++,+.
   .########.
   .,,,,,,,,.
   ..........




                """)
M('ROOF_HATCH', hatch, 'B', solid=True)
chim = quad(CORR, CORR2, CORR, CORR2).stamp(0, 0, """

    ........
   .########.
  .#+,....,+#.
  .#,......,#.
  .#,......,#.
  .#+,....,+#.
   .########.
   .,+,,,,+,.
   .,+,,,,+,.
   .,+,,,,+,.
   .,+,,,,+,.
   ..........


                """)
M('ROOF_CHIMNEY', chim, 'B', solid=True)
bigvent = quad(CORR, CORR2, CORR, CORR2).stamp(0, 0, """

  ............
 .############.
 .+..........+.
 .+.########.+.
 .+.+,,,,,,+.+.
 .+.########.+.
 .+..........+.
 .############.
 .,,,,,,,,,,,,.
 .,,,,,,,,,,,,.
 .,..,..,..,.,.
  ............


                """)
M('ROOF_VENT', bigvent, 'B', solid=True)
beacon = on_roof("""


    ........
    .,,,,,,.
    .,.##.,.
    .,.##.,.
    .,,,,,,.
    ........
      .++.
      .++.
      .++.
     .,++,.
    ..,++,..
     ......

                """)
M('ROOF_BEACON', beacon, 'SSBB', solid=True, blink='B')
sap = Canvas(16, 16, 1).blit(quad(MOSS1, MOSS2, MOSS2, MOSS1), 0, 0).stamp(0, 0, """

     ,,,,,
   ,,++#++,,
  ,++#++++++,
  ,+++,++,+++,
  ,,++++++++,,
   ,,,+++++,,
    .,,,,,,.
      .,,.
      .,,.
     ......




                """)
M('ROOF_SAPLING', sap, 'P', solid=True)

# ================================================== FACADES (BUILD family)
WALL_T = T("""
,,,,,,,,
,,,,,,,,
,,,,,,,,
++++++++
,,,,,,,,
,,,,,,,,
,,,,,,,,
........""")


def facade(art, base=None):
    c = quad(WALL_T) if base is None else base
    return c.stamp(0, 0, art)


win2 = facade("""
................
,,,,,,,,,,,,,,,,
,.....,,,.....,,
,.###.,,,.###.,,
,.#++.,,,.#++.,,
,.+++.,,,.+++.,,
,.+++.,,,.+++.,,
,.+++.,,,.+++.,,
,.+++.,,,.+++.,,
,.....,,,.....,,
,+++++,,,+++++,,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
win2b = facade("""
................
,,,,,,,,,,,,,,,,
,.....,,,.....,,
,.###.,,,.###.,,
,.+++.,,,.,,,.,,
,.,,,.,,,.+++.,,
,.+++.,,,.,,,.,,
,.,,,.,,,.+++.,,
,.+++.,,,.,,,.,,
,.....,,,.....,,
,+++++,,,+++++,,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
for art, tag in ((win2, ''), (win2b, 'B')):
    M(f'WIN{tag}_LL', art, 'WWWW', solid=True)
    M(f'WIN{tag}_LD', art, 'WBWB', solid=True)
    M(f'WIN{tag}_DL', art, 'BWBW', solid=True)
    M(f'WIN{tag}_DD', art, 'B', solid=True)
balc = facade("""
................
,,,,,,,,,,,,,,,,
,.....,,,.....,,
,.###.,,,.###.,,
,.#++.,,,.#++.,,
,.+++.,,,.+++.,,
,.+++.,,,.+++.,,
................
################
.+.+.+.+.+.+.+.+
.+.+.+.+.+.+.+.+
.+.+.+.+.+.+.+.+
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
M('BALC_L', balc, 'WWBB', solid=True)
M('BALC_D', balc, 'B', solid=True)
M('BALC_LD', balc, 'WBBB', solid=True)
wall = facade("""
................
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,.,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,.,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
M('WALL', wall, 'B', solid=True)
door = facade("""
................
,,,,,,,#,,,,,,,,
,,,,,,+#+,,,,,,,
,,,,,,,,,,,,,,,,
,,,,.......,,,,,
,,,,.+++++.,,,,,
,,,,.+###+.,,,,,
,,,,.+#++#.,,,,,
,,,,.+#++#.,,,,,
,,,,.+#++#.,,,,,
,,,,.+#++#.,,,,,
,,,,.+#++#.,,,,,
,,,,.+#++#.,,,,,
,,,,.+####.,,,,,
,,,++++++++++,,,
................""")
M('DOOR', door, 'W', solid=True)
M('DOOR_DARK', door, 'B', solid=True)
shop = facade("""
................
++++++++++++++++
................
.##############.
.#++++++.+++++#.
.#+,,,,+.+,,,+#.
.#+.,.,+.+.,.+#.
.#++++++.+++++#.
.#+,.,,+.+,.,+#.
.#+,,,,+.+,,,+#.
.#++++++.+++++#.
.#+,,,,+.+,,,+#.
.#+++++,.+++++#.
.##############.
................
................""")
M('SHOP_W', shop, 'W', solid=True)
M('SHOP_N', shop, 'N', solid=True)
shut = facade("""
................
++++++++++++++++
................
.++++++++++++++.
.,,,,,,,,,,,,,,.
.++++++++++++++.
.,,,,,,,,,,,,,,.
.++++++++++++++.
.,,,,,,,,,,,,,,.
.++++++++++++++.
.,,,,,,,,,,,,,,.
.++++++++++++++.
.,,,,,,,,,,,,,,.
.+++++,,,,+++++.
.......##.......
................""")
M('SHUTTER', shut, 'B', solid=True)
gar = facade("""
................
,,,,,,,,,,,,,,,,
.##############.
.+,,,,,,,,,,,,+.
.++++++++++++++.
.+,,,,,,,,,,,,+.
.++++++++++++++.
.+,,,,,,,,,,,,+.
.++++++++++++++.
.+,,,,,,,,,,,,+.
.++++++++++++++.
.#..##..##..##..
.##..##..##..##.
.+,,,,,,,,,,,,+.
................
................""")
M('GARAGE', gar, 'B', solid=True)
wvent = facade("""
................
,,,,,,,,,,,,,,,,
,..............,
,.############.,
,.+..........+.,
,.############.,
,.+..........+.,
,.############.,
,.+..........+.,
,.############.,
,..............,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
M('WALL_VENT', wvent, 'B', solid=True)
warn = facade("""
................
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,......,,,,,,,
,,,.####.,,,,,,,
,,,.#++#.,,,,,,,
,,,.#++#.,,,,,,,
,,,.####.,,,,,,,
,,,......,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
++++++++++++++++
.#..#..#..#..#..
#..#..#..#..#..#
................""")
M('WALL_WARN', warn, 'SBBB', solid=True, blink='B')
wpipe = facade("""
................
,,.++.,,,,.++.,,
,,.#+.,,,,.#+.,,
,,.#+.,,,,.#+.,,
,..#+..,,..#+..,
,.####.,,.####.,
,..#+..,,..#+..,
,,.#+.,,,,.#+.,,
,,.#+.,,,,.#+.,,
,,.#+.,,,,.#+.,,
,,.#+.,,,,.#+.,,
,,.#+.,,,,.#+.,,
++.#+.++++.#+.++
,,.#+.,,,,.#+.,,
,,.++.,,,,.++.,,
................""")
M('WALL_PIPES', wpipe, 'B', solid=True)
steel = facade("""
................
,,,,,,,,,,,,,,,,
,,,,..........,,
,,,,.########.,,
,,,,.+.,,,,.+.,,
,,,,.+,,,,,,+.,,
,,,,.+,,,,,,+.,,
,,,,.+,,,,#,+.,,
,,,,.+,,,,,,+.,,
,,,,.+,,,,,,+.,,
,,,,.+.,,,,.+.,,
,,,,.++++++++.,,
,,,,.,,,,,,,,.,,
,,,,..........,,
++++++++++++++++
................""")
M('STEEL_DOOR', steel, 'B', solid=True)
vines = facade("""
.,+#+...........
,+#+,,,,,,,,,+#,
,,+#+,,,,,,,+#+,
,,,+#,.....,,+,,
,,+#+,.###.,,,,,
,+#+,,.#++.,,+,,
,,+,,,.+++.,+#+,
,,,,,,.+++.,,+#,
,,,,,,.....,+#+,
,,,,,,,,,,,,,+,,
,,,,,,,,,,,,,,,,
,+,,,,,,,,,,,,,,
++#+++++++++++++
,+#+,,,,,,,,,,,,
,,+,,,,,,,,,,,,,
................""")
M('WALL_VINES', vines, 'P', solid=True)
broken = facade("""
................
,,,,,,,,,,,,,,,,
,.....,,,.....,,
,.#,..,,,.##+.,,
,.,.+.,,,.+,..,,
,..,..,,,.+...,,
,.+.,.,,,.+.,.,,
,.,..+,,,..,..,,
,.+.,.,,,.,..+,,
,.....,,,.....,,
,+++++,,,+++++,,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
M('WIN_BROKEN', broken, 'B', solid=True)
strip = facade("""
................
,,,,,,,,,,,,,,,,
................
.##+##+##+##+##.
.++.++.++.++.++.
................
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
++++++++++++++++
,,,,,,,,,,,,,,,,
,,,,,,,,,,,,,,,,
................""")
M('STRIP_L', strip, 'WWBB', solid=True)
M('STRIP_D', strip, 'B', solid=True)
lobby = facade("""
................
++++++++++++++++
................
.##############.
.#++++++#++++++.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#+,,,,+#+,,,,+.
.#++++++#++++++.
.##############.
................
................""")
M('LOBBY', lobby, 'W', solid=True)

# ===================================================================== SIGNS
FONT3_ROWS = {
    'A': ".#. #.# ### #.# #.#", 'B': "##. #.# ##. #.# ##.", 'C': ".## #.. #.. #.. .##",
    'D': "##. #.# #.# #.# ##.", 'E': "### #.. ##. #.. ###", 'F': "### #.. ##. #.. #..",
    'G': ".## #.. #.# #.# .##", 'H': "#.# #.# ### #.# #.#", 'I': "### .#. .#. .#. ###",
    'J': "..# ..# ..# #.# .#.", 'K': "#.# #.# ##. #.# #.#", 'L': "#.. #.. #.. #.. ###",
    'M': "#.# ### ### #.# #.#", 'N': "##. #.# #.# #.# #.#", 'O': ".#. #.# #.# #.# .#.",
    'P': "##. #.# ##. #.. #..", 'Q': ".#. #.# #.# ##. .##", 'R': "##. #.# ##. #.# #.#",
    'S': ".## #.. .#. ..# ##.", 'T': "### .#. .#. .#. .#.", 'U': "#.# #.# #.# #.# ###",
    'V': "#.# #.# #.# #.# .#.", 'W': "#.# #.# ### ### #.#", 'X': "#.# #.# .#. #.# #.#",
    'Y': "#.# #.# .#. .#. .#.", 'Z': "### ..# .#. #.. ###", '0': "### #.# #.# #.# ###",
    '1': ".#. ##. .#. .#. ###", '2': "##. ..# .#. #.. ###", '3': "##. ..# .#. ..# ##.",
    '4': "#.# #.# ### ..# ..#", '5': "### #.. ##. ..# ##.", '6': ".## #.. ### #.# ###",
    '7': "### ..# .#. .#. .#.", '8': "### #.# ### #.# ###", '9': "### #.# ### ..# ##.",
    '-': "... ... ### ... ...", '.': "... ... ... ... .#.", ' ': "... ... ... ... ...",
    '/': "..# ..# .#. #.. #..",
}
FONT3 = {k: v.replace(' ', '') for k, v in FONT3_ROWS.items()}
assert all(len(v) == 15 for v in FONT3.values())


def text_width(s):
    return len(s) * 4 - 1


def sign(text, pal, blink=''):
    lines = text.split('/')
    w = max(text_width(l) for l in lines) + 6
    mw = (w + 15) // 16
    c = Canvas(mw * 16, 16, 0)
    W = mw * 16
    # mount and neon tube frame
    c.hline(1, 0, W - 2, 1).hline(1, 15, W - 2, 1)
    c.vline(0, 1, 14, 1).vline(W - 1, 1, 14, 1)
    tops = [5] if len(lines) == 1 else [2, 9]
    if len(lines) == 1:
        c.hline(2, 2, W - 4, 2).hline(2, 13, W - 4, 2)
    for line, top in zip(lines, tops):
        x0 = (W - text_width(line)) // 2
        for i, ch in enumerate(line):
            g = FONT3[ch]
            for gy in range(5):
                for gx in range(3):
                    if g[gy * 3 + gx] == '#':
                        c.px(x0 + i * 4 + gx, top + gy, 3)
    return c


SIGNS = [
    ('REST', 'REST IS/OPTIONAL', 'N'),
    ('MEMORY', 'MEMORY/SERVICES', 'N'),
    ('QUIET', 'PUBLIC/QUIET ZONE', 'N'),
    ('AVAIL', 'STAY/AVAILABLE', 'S'),
    ('VACANCY', 'VACANCY', 'S'),
    ('OPEN24', 'OPEN/24H', 'N'),
    ('NOODLES', 'NOODLES', 'S'),
    ('LAUNDRY', 'LAUNDRY', 'N'),
    ('PHARMACY', 'PHARMACY', 'N'),
    ('NOSERVICE', 'NO/SERVICE', 'S'),
    ('VOLTAGE', 'HIGH/VOLTAGE', 'S'),
    ('PARKING', 'PARKING', 'N'),
]
SIGN_INFO = []
for key, text, pal in SIGNS:
    c = sign(text, pal)
    n = c.w // 16
    blink = 'B' if pal == 'S' else ''
    for i in range(n):
        M(f'SIGN_{key}{i}', c.sub(i * 16, 0, 16, 16), pal, solid=True, blink=blink)
    SIGN_INFO.append((key, n, text))

# ===================================================== special / landmarks
# "MEMORY SERVICES" style billboard frame on legs, for rooftop placement
M('VOID', Canvas(16, 16, 0), 'G', solid=True)


def write():
    out = ROOT / 'assets/tiles'
    canv = [m['canvas'] for m in MT]
    save_gray_png(out / 'metatiles.png', canv, 16, 16, 16)
    meta = [{k: v for k, v in m.items() if k != 'canvas'} for m in MT]
    doc = {
        '_comment': 'Metatile definitions for assets/tiles/metatiles.png (16 per row, '
                    'in this order). pals = palette per 8x8 quadrant TL,TR,BL,BR: '
                    'G GROUND B BUILD W WINDOW N NEON S SIGNAL P NATURE A WATER L LAMP. '
                    'prio = BG-over-sprite per quadrant. blink = palette used when the '
                    'engine switches the light off. Generated by '
                    'tools/asset_generation/paint_world.py.',
        'metatiles': meta,
        'signs': [{'key': k, 'width': n, 'text': t} for k, n, t in SIGN_INFO],
    }
    (out / 'metatiles.json').write_text(json.dumps(doc, indent=1))
    frames = []
    for tag, fr in ANIMS:
        row = Canvas(8 * 4, 8, 0)
        for i, f in enumerate(fr):
            row.blit(f, i * 8, 0)
        frames.append(row)
    save_gray_png(out / 'animations.png', frames, 1, 32, 8)
    (out / 'animations.json').write_text(json.dumps(
        {'_comment': 'One row per animated 8x8 tile; frame 0 must appear in metatiles.png.',
         'anims': [{'tag': t, 'frames': len(f)} for t, f in ANIMS]}, indent=1))
    # train tiles
    tr = Canvas(8 * 3, 16)
    for i, k in enumerate(('nose', 'body', 'gap')):
        tr.blit(TRAIN[k][0], i * 8, 0).blit(TRAIN[k][1], i * 8, 8)
    save_gray_png(ROOT / 'assets/sprites/train.png', [tr], 1, 24, 16)
    print(f'paint_world: {len(MT)} metatiles, {len(ANIMS)} animations')


if __name__ == '__main__':
    write()
