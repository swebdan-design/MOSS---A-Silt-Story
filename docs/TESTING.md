# Testing

## How to run

```sh
make test                                       # host tests + ROM header
python3 tests/emu_playthrough.py                # full playthrough in PyBoy (~5 min)
MOSS_EMU=sameboy python3 tests/emu_playthrough.py   # same on SameBoy (see BUILD.md)
python3 tests/emu_systems.py                    # saves, corruption, 25-minute pond session
```

## Latest results

| Area | Result | Evidence |
| --- | --- | --- |
| ROM header, cartridge type, checksums, DMG-only flag | PASS | tools/verify_rom.py |
| Composer, garden, memory (host) | PASS | tests/host_compose.c |
| Save format, corruption, 20 000 fuzz images (host) | PASS | tests/host_save.c |
| Full story playthrough, PyBoy | PASS | tests/emu_playthrough.py |
| Full story playthrough, SameBoy (DMG, cycle-accurate) | PASS | MOSS_EMU=sameboy tests/emu_playthrough.py |
| Saves, power cycle, recovery, corrupted SRAM, pond cycles, long session (PyBoy) | PASS | tests/emu_systems.py |
| Physical DMG / flash cartridge | NOT TESTED | no hardware available |
| BGB, Emulicious, mGBA, Gambatte | NOT TESTED | not available in the build environment |
| Playtime with human players | NOT TESTED | estimate only (see KNOWN_LIMITATIONS) |

The full report with every check is `release/testing/TESTING_REPORT.md`.

## Tools

- `tools/play.py` — automated player: BFS path-finding on the collision map in
  RAM, dialogue handling, warps.
- `tools/emu.py`, `tools/pyboy_util.py` — PyBoy session, audio recorder.
- `tools/sameboy_backend.py`, `tools/sameboy_shim.c` — the same interface on
  the SameBoy core, with 16-bit audio capture.
- `tools/savefile.py` — builds cartridge RAM images (valid, damaged, post-game).
- `tools/symbols.py` — reads GBDK's `.noi` symbol file for state checks.
