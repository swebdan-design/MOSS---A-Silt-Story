# Building MOSS

## Requirements

| Tool | Version used for this release | Needed for |
| --- | --- | --- |
| GBDK-2020 | 4.3.0 (bundles SDCC 4.4.1 #14650, sm83 port) | compiling the ROM |
| GNU Make | 4.x | build orchestration |
| Python | 3.10+ (tested 3.13.18), standard library only | tile/map/font/picture compiler |
| A host C compiler | GCC 13.3 (any C99 compiler) | host tests |

Optional, only for testing and publishing (never needed to compile the ROM):
PyBoy 2.9.0 and NumPy (emulator tests); SameBoy 1.0.1 built as `libsameboy`
plus RGBDS 0.9.1 to build its boot ROMs (second emulator, soundtrack capture);
Pillow 12 (artwork, contact sheets), ReportLab 5 (manual PDF), FFmpeg 6
(FLAC/MP3 encoding), DejaVu fonts.

## Toolchain

```sh
sh tools/bootstrap_toolchain.sh
```

downloads `gbdk-linux64.tar.gz` for GBDK-2020 4.3.0 from the official GitHub
release, verifies SHA-256
`5032e4e4edcef3f0ed232c895bbd211973963329304ff2c030ea34d0b2d964a4` and
extracts it to `.toolchain/gbdk`. To use an existing installation instead:
`make GBDK_HOME=/opt/gbdk`. On macOS or Windows use the official GBDK-2020
4.3.0 release archive for your platform and point `GBDK_HOME` at it.

## Commands

```sh
make                 # build release/MOSS_by_Synth_Centric.gb (and build/moss.map, .noi)
make test            # host tests (composer, garden, memory, saves) + ROM header verification
make test-emulator   # full automated playthrough in PyBoy (about 5 minutes)
make clean           # remove build/
```

What `make` does:

1. `tools/gen_audio_tables.py` writes the DMG period table and wave patterns.
2. `tools/gen_assets.py` compiles `assets/` (ASCII pixel art, maps, font) and
   `tools/art_scenes.py` (title, ending, notebook pictures) into C data,
   checks every hardware budget (144 BG tiles per map, 128 sprite tiles,
   picture budgets) and packs data into ROM banks 3–9. Previews go to
   `build/preview/`.
3. `lcc` compiles `source/*.c` and the generated data. Code banks are fixed
   with `#pragma bank`: 0 home (frame loop, audio interrupt, VRAM, text,
   saves), 1 composer/garden/memory, 2 overworld, 10 story flow, 11 people,
   12 places and puzzles, 13 notebook and menus, 14 title and ending, 15 pond
   instrument.
4. The link uses `-Wm-yt0x1B -Wm-ya1 -Wm-yoA -Wm-yn"MOSS"`: MBC5+RAM+BATTERY,
   one 8 KiB SRAM bank, ROM size chosen automatically (256 KiB), title "MOSS",
   no CGB flag. `build/romusage.txt` records bank usage.

## Second emulator and soundtrack (optional)

```sh
git clone --depth 1 --branch v1.0.1 https://github.com/LIJI32/SameBoy.git
cd SameBoy && PATH=/path/to/rgbds:$PATH make lib tester
export SAMEBOY_LIB=$PWD/build/lib/libsameboy.so SAMEBOY_BOOT=$PWD/build/bin/tester/dmg_boot.bin
cd /path/to/MOSS
cc -shared -fPIC -O2 tools/sameboy_shim.c -L$(dirname $SAMEBOY_LIB) -lsameboy -o build/tools/libmoss_shim.so
MOSS_EMU=sameboy python3 tests/emu_playthrough.py   # same playthrough on SameBoy
python3 tools/capture_soundtrack.py                 # records the 10 tracks from the ROM
```

## Publishing (optional)

```sh
python3 tools/create_artwork.py     # covers, back cover, banner, label mock-up, SVGs
python3 tools/make_world_map.py     # docs/world_map.png
python3 tools/create_manual_pdf.py  # release/manual/MOSS_Manual.pdf from the Markdown
python3 tools/package_release.py    # release folder copies, SHA256SUMS, dist/ ZIPs
```

The build is deterministic: the same toolchain produces the same ROM bytes
(checked by rebuilding from a clean tree; see `release/testing/BUILD_REPORT.md`).
