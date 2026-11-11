# VACANT — Architecture

VACANT is written in C for GBDK-2020 4.5.0 (SDCC 4.5.1, SM83 port). There is
no assembly in the project; time-critical paths are kept short and run from the
home bank.

## Source layout

```
src/
  core/      main.c (start-up, VBlank interrupt, exploration loop), game.h, platform.h
  player/    player.c - grid movement, camera, map streaming, walker sprite
  world/     worldgen.c - deterministic city generator and cell cache
             render.c   - metatiles -> hardware map, blinking lights, trains
  procedural/ (hashing and RNG live in worldgen.c; see WORLD_GENERATION.md)
  graphics/  palette.c - palette engine; gfx.c - tile upload and tile animation
  weather/   weather.c - weather states, rain sprites, lightning
  audio/     sound.c   - Game Boy Color sound hardware layer
  music/     music.c   - generative composer
  ui/        ui.c      - text, district popups, synthesiser pages, title screen
  save/      save.c    - dual-slot battery saves
assets/      editable source art (PNG + JSON) and palettes
tools/       asset_generation/ (painters, compiler, audio tables, cover)
             audio_preview/    (capture the ROM's sound to WAV)
             validation/       (ROM header check, emulator wrapper, screenshots, maps, saves)
tests/       host_world.c, host_music.c, host_save.c (PC), emu_tests.py (PyBoy)
```

## ROM banks (MBC5, 128 KiB)

| Bank | Contents | Used |
| --- | --- | --- |
| 0 (home) | start-up, VBlank interrupt, palette engine, sound layer, composer, saves, player, palettes | 14.7 KiB / 16 KiB |
| 1 | city generator, cell cache logic, collision flags | 13.7 KiB |
| 2 | world, sprite and train tile graphics, tile animation frames and code | 7.6 KiB |
| 3 | interface code, font, title picture | 10.3 KiB |
| 4 | weather | 1.0 KiB |
| 5 | renderer, blinking lights, trains, metatile tables | 3.9 KiB |

Banked functions are declared `BANKED`; GBDK's trampolines switch and restore
the ROM bank. Rules followed throughout:

- **The interrupt never touches a switchable bank.** Everything the VBlank
  handler calls (palette upload, `music_tick`, sound layer, their tables)
  lives in bank 0, so it is correct whichever bank the main program has mapped.
- **No pointer into a switchable bank is returned to a different bank.** Text
  shown by the interface lives either in bank 0 or in bank 3 itself.
- Data used by a banked function lives in the same bank (bank 1 and bank 5
  each carry their own copy of the metatile tables they need).

## Memory

- WRAM: about 4.7 KiB of variables. The largest is the cell cache: six cells of
  up to 20 x 20 metatiles (2.5 KiB), plus 1.2 KiB of flood-fill scratch.
  Nothing is allocated dynamically; exploration never consumes more memory.
- Stack: lowest observed SP in a 30-minute emulator session was 0xDFB3.
- VRAM bank 0: tiles 0-191 city, 192-255 interface font.
  VRAM bank 1: tiles 0-47 sprites (walker, rain), 48-249 city, 250-255 train.
  The LCD uses 0x8000 tile addressing for both background and sprites, so
  every tile upload goes through `set_sprite_data` (0x8000-based).
- SRAM: two 122-byte save slots at 0xA000 and 0xA100.

## Frame structure

The CPU runs in double-speed mode. The VBlank interrupt (59.73 Hz):

1. writes the scroll and window registers from shadows (`scx_v` …),
2. uploads palettes if the palette engine produced new ones,
3. advances the composer (`music_tick`).

GBDK's own VBlank work (shadow OAM DMA) runs first. Because the music is driven
by the interrupt, it keeps perfect time even when the main loop takes longer
than a frame (for example while a new city block is generated).

The main loop, once per frame: read the joypad; open the synthesiser or toggle
modes; advance the walker (which draws one new strip of the map at the start
of each step); update blinking lights; animate tiles; update weather and rain
sprites; maybe start or advance a train; update the district popup; recompute
palettes if anything changed. All VRAM writes go through GBDK routines that wait
for accessible LCD modes, so they are safe outside VBlank.

## Map streaming

World metatile (x, y) is always drawn at hardware map tile
((x & 15) * 2, (y & 15) * 2). The 32 x 32 tile map is therefore a 16 x 16
metatile ring buffer that wraps exactly with world coordinates. The region
[cam_x, cam_x + 10] x [cam_y, cam_y + 9] is kept drawn; at the start of a
step one column (10 metatiles) or row (11 metatiles) is added on the side the
camera moves toward. A full redraw happens only when entering a city, hidden by
a fade. The emulator tests read both VRAM banks and compare every visible map
entry with the host generator (0 mismatches over 40 checks after 160 random
walks and across the world seam).

## Random-number streams

| Stream | Where | Seeded from |
| --- | --- | --- |
| geometry | stateless `hash3(seed, x, y, salt)` + per-cell xorshift | world seed and cell coordinates |
| music | xorshift16 `mrs` in music.c | musical seed (presets carry their own) |
| visual | `vrs` (render.c), `vis` (main.c), `wrs` (weather.c) | constants and the divider register |

The visual streams never affect geometry or music, and the music stream never
affects geometry.
