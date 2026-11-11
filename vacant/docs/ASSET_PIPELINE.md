# VACANT — Asset pipeline

```
tools/asset_generation/paint_*.py   (how the original art was made)
          |  make paint
          v
assets/   editable source: PNG sheets + JSON definitions + palettes.json
          |  make (build_assets.py)
          v
build/gen/  C data: assets.h, home_data.c, world_data.c, render_data.c, gfx_data.c, ui_data.c
          |  lcc
          v
release/VACANT_by_Synth_Centric.gbc
```

## Source files

| File | Content |
| --- | --- |
| `assets/palettes/palettes.json` | the eight background palettes, district wall tints, sprite and UI palettes |
| `assets/tiles/metatiles.png` | every 16 x 16 city metatile, 16 per row, in 4-level grey (black = shade 0 … white = shade 3) |
| `assets/tiles/metatiles.json` | per metatile, same order: `name`, `pals` (palette per 8 x 8 quadrant TL TR BL BR), `solid`, `prio` (drawn above sprites), `blink` (palette used when the light is off) |
| `assets/tiles/animations.png/.json` | one row per animated 8 x 8 tile: up to four frames; frame 0 must also appear in metatiles.png |
| `assets/sprites/player.png` | nine 16 x 16 walker frames (down, up, right x stand, walk A, walk B); shade 0 is transparent |
| `assets/sprites/rain.png` | four 8 x 16 frames: two streaks, two splashes |
| `assets/sprites/train.png` | 3 x 2 tiles: nose, body, coupling (top and bottom rows) |
| `assets/interface/font.png/.json` | 64 interface glyphs; `chars` maps index to character (a-e meter cells, f cursor, …) |
| `assets/interface/title.png/.json` | 160 x 144 title picture; `tile_palette` gives the palette of each 8 x 8 tile |

Palette letters: G GROUND, B BUILD, W WINDOW, N NEON, S SIGNAL, P NATURE, A WATER, L LAMP.

### The palette families

All eight palettes share shade 0, so tiles of different palettes always meet
cleanly. GROUND, LAMP and SIGNAL share shade 1 (the pavement), BUILD, WINDOW and
NEON share shade 1 (the wall, tinted per district at runtime). Shade 2 of a
family palette is the "lit surface" tone. This is how lights work: a lamp
metatile is ordinary pavement in the LAMP palette, so its slab bevels turn warm;
a shopfront in NEON turns the wall highlights cyan.

When drawing a quadrant in a special palette (WINDOW, NEON, SIGNAL, LAMP), use
shades 0/1 for anything that must match its neighbours and shades 2/3 only for
light.

## Changing or adding art

1. Edit `assets/tiles/metatiles.png` in any pixel editor using only the four
   greys (0, 85, 170, 255). Keep the 16-per-row grid.
2. Change palettes, solidity or blinking in `metatiles.json`.
3. To add a metatile, append a cell to the PNG and an entry to the JSON; refer
   to it in C as `MT_<NAME>` (generated in `build/gen/assets.h`).
4. `make`. The compiler deduplicates 8 x 8 tiles (including mirrored copies) and
   fails with a message if VRAM is exceeded: 394 city tiles are available
   (currently 389 used).

`make paint` regenerates the source art from the painters and overwrites hand
edits; use one workflow or the other.

## Budgets enforced by build_assets.py

| Resource | Limit | Used |
| --- | --- | --- |
| metatiles | 255 | 194 |
| city tiles (both VRAM banks) | 394 | 389 |
| sprite tiles | 48 | 44 |
| interface glyphs | 64 | 64 |
| title tiles | 400 | 212 |

## Other generated data

- `gen_audio_tables.py` -> `build/gen/audio_tables.inc`: note periods (MIDI 36-119) and wave timbres.
- `paint_cover.py` -> `artwork/VACANT_cover_250.png` (native) and `artwork/VACANT_Album_Cover.png` (3000 x 3000).
