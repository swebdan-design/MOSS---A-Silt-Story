# Save system

Cartridge: **MBC5 + RAM + battery**, one 8 KiB SRAM bank. MOSS uses two
1 KiB slots at `0xA000` and `0xA400`. Source: `source/save.c`, `source/save.h`.

## Record (184 bytes, version 1)

| Offset | Field | Notes |
| --- | --- | --- |
| 0 | magic `0x4D53` ("MS") | |
| 2 | version | 1 |
| 3 | sequence | +1 every write; decides the newest copy |
| 4 | day (1–6), postgame | |
| 6 | map, x, y, facing | safe position (only saved while walking) |
| 10 | flags[8] | 64 story flags |
| 18 | found, found_count, found_order[8] | discoveries and their order (notebook pages) |
| 28 | seed, minutes | composer seed |
| 32 | settings | text speed, listening time, mono, volume |
| 36 | pond (50 B) | the player's garden and pond settings |
| 86 | my_pond (50 B) | post-game "Keep my pond" slot |
| 136 | my_pond_saved, preset | |
| 138 | music memory (40 B) | see MUSICAL_MEMORY.md |
| 178 | reserved (4 B) | |
| 182 | checksum | Fletcher-16 variant over bytes 0–181 |

## Integrity and recovery

- A copy is valid only if magic, version and checksum match **and** every
  value is in range (day, map, facing, lily types, scale, density). A record
  with a correct checksum but impossible contents is rejected.
- Writes **alternate** between the two slots and always go to the older one.
  If power is lost mid-write, the other copy is untouched. After writing, the
  record is read back and validated.
- On loading, the newest valid copy is used. If the newer copy is damaged, the
  older one is loaded and the title screen says, once: "The newest page of the
  notebook was smudged. The page before it was still clear…". If neither copy
  is valid but data is present, it says the notebook "couldn't be read" and a
  new summer can begin. A malformed save is never interpreted.
- Unknown versions are not loaded (a future version would convert or ignore).

## When the game saves

- Pause menu → Save (any time while walking).
- Automatically: after every discovery, at the start of each new day, at the
  ending, when "Keep my pond" is chosen, and when leaving the post-game pond.

## Emulators and flash carts

Emulators store SRAM in a battery file next to the ROM; names differ:
SameBoy and BGB `MOSS_by_Synth_Centric.sav`, mGBA `.sav`, PyBoy `.gb.ram`,
Emulicious `.sav`. Rename the file if moving a save between emulators.
Flash carts need MBC5 with 8 KiB battery SRAM support; some write the save to
their SD card only when you return to their menu or press reset.

## Tests

`tests/host_save.c`: blank (0xFF) and zeroed SRAM, round trip, alternation,
interrupted write, both copies damaged, wrong version, valid checksum with
impossible values, and 20 000 random or corrupted SRAM images. 
`tests/emu_systems.py` repeats the important cases on the real ROM (power
cycle, recovery message, corrupted cartridge, random garbage).
