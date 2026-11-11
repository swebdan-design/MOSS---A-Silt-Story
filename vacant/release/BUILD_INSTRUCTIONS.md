# VACANT — Build instructions

All paths are relative to the root of the source package (the `vacant/`
directory in the repository, or the top of `VACANT_Source.zip`).

## Requirements

| Tool | Version used | Needed for |
| --- | --- | --- |
| GBDK-2020 | 4.5.0 (bundles SDCC 4.5.1 #15267, SM83 port) | compiling the ROM |
| GNU Make | 4.x | build orchestration |
| Python | 3.10+ with Pillow (tested 3.13, Pillow 12.3) | asset compiler and audio tables |
| A C compiler | GCC 13 or any C99 compiler | host tests |

Optional, for tests and the release package only: PyBoy 2.9 and NumPy
(emulator tests, audio capture, screenshots), ReportLab 5 (manual PDF).

## Toolchain

```bash
sh tools/bootstrap_toolchain.sh
```

downloads `gbdk-linux64.tar.gz` for GBDK-2020 4.5.0 from the official GitHub
release, checks its SHA-256
(`d7857a5f6d135ee4c249043ca26aad9f2ec8ab5d4106d97720d404114f42605c`) and
unpacks it into `.toolchain/gbdk`. To use an existing installation instead:

```bash
make GBDK_HOME=/path/to/gbdk
```

On macOS or Windows use the GBDK-2020 4.5.0 archive for your platform.

## Build

```bash
make
```

1. `tools/asset_generation/build_assets.py` compiles `assets/` into
   `build/gen/*.c` and checks every VRAM budget;
2. `tools/asset_generation/gen_audio_tables.py` writes the note-period table and
   wave timbres;
3. `lcc` compiles `src/` and the generated data and links
   `release/VACANT_by_Synth_Centric.gbc` with
   `-Wm-yt0x1B -Wm-ya1 -Wm-yC -Wm-yoA` (MBC5 + RAM + battery, 8 KiB SRAM,
   Game Boy Color only, ROM size chosen by the linker: 128 KiB);
4. `tools/validation/verify_rom.py` checks the header (logo, title, CGB flag,
   cartridge type, ROM and RAM size, header and global checksums).
   `build/romusage.txt` lists bank usage.

## Tests

```bash
make host-test
```

```bash
make test-emulator
```

`host-test` runs the generator, composer and save tests on the PC (seconds).
`test-emulator` runs `tests/emu_tests.py` against the ROM in PyBoy (about 90 s;
add `--quick` when running the script directly for a shorter session).

## Other targets and tools

| Command | Result |
| --- | --- |
| `make paint` | regenerate source art from the painters (overwrites hand edits) |
| `python3 tools/asset_generation/paint_cover.py` | album cover |
| `python3 tools/validation/screenshots.py` | emulator screenshots in `build/shots/` |
| `python3 tools/audio_preview/capture_rom_audio.py out.wav 3 --walk` | record the ROM's audio |
| `python3 tools/create_manual_pdf.py` | `build/release/VACANT_Manual.pdf` |
| `python3 tools/package_release.py` | assemble `release/` and the ZIP archives |
| `make clean` | remove `build/` |

## Reproducibility

The build is deterministic: the same toolchain produces byte-identical ROMs.
This was checked by extracting `VACANT_Source.zip` into an empty directory,
building there, and comparing SHA-256 sums (see `TESTING_REPORT.md`).
