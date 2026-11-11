# VACANT — Testing report

All results below come from the release ROM
`VACANT_by_Synth_Centric.gbc`
(SHA-256 `3717c4cc506b189e2dd8eb0791d8e8c55b927741faa322bf1ea788cc1424b970`),
built with GBDK-2020 4.5.0 / SDCC 4.5.1.

**Not tested:** physical Game Boy Color, Game Boy Advance, flash cartridges,
and emulators other than PyBoy. See `KNOWN_LIMITATIONS.md`.

## 1. ROM header (tools/validation/verify_rom.py)

| Check | Result |
| --- | --- |
| Nintendo logo | correct |
| Title | `VACANT` |
| CGB flag | 0xC0 (Game Boy Color only) |
| Cartridge type | 0x1B MBC5 + RAM + BATTERY |
| ROM size | code 2 = 128 KiB, matches file size |
| RAM size | code 2 = 8 KiB |
| Header and global checksums | correct |

ROM bank usage: bank 0 90%, bank 1 85%, bank 2 48%, bank 3 64%, bank 4 6%,
bank 5 25%. WRAM: about 4.7 KiB of variables.

## 2. Host tests (`make host-test`, PC)

**City generator (tests/host_world.c)** — all checks passed

| Test | Result |
| --- | --- |
| Determinism | a 640 x 640 window generated in three different visiting orders (row-major, reverse, random probes) gives the same checksum |
| Seeds differ | seeds 1 and 2 differ in 88.4% of metatiles |
| Torus seam | a window across x = y = 65535/0 generates valid metatiles; the street line at x = 0 is unbroken |
| Connectivity | from the start position, every walkable metatile in a 640 x 640 window (border excluded) is reachable, for seeds 1, 4242, 65535, 31337 (about 216,000-221,000 each, 0 unreachable) |
| Districts (25,600 cells) | streets 26.3%, industrial 20.8%, residential 19.6%, lower 13.6%, forgotten 19.7% |
| Landmarks / repairs (3,600 cells) | landmarks in 19.8% of cells; 328 enclosed pockets repaired in 102 cells (2.8%) |

**Composer (tests/host_music.c)** — all checks passed

| Test | Result |
| --- | --- |
| 5 presets x 5 districts, 10 simulated minutes each | every combination produces melody (55-300 notes), pulse, drone, noise where expected; melody within MIDI 36-91; longest rests 15-60 s; no drone retrigger storms |
| Presets differ | every preset's activity differs from EMPTY STREETS |
| District change | no drone restart within 2 s of crossing a boundary |
| Landmarks | near (distance 2) produces more events than far (distance 40) for each landmark type |
| 8 simulated hours | 872-1042 melody notes every hour; 50 mode changes and 50 tonal-centre changes |
| Randomise | 500 random configurations, all parameters within range |
| Freeze harmony | chord and key unchanged for 10 minutes |

**Saves (tests/host_save.c)** — all checks passed: blank SRAM reported empty;
alternating slots, newest chosen; a power cut half-way through a write recovers
the previous save (status RECOVERED); one corrupted slot falls back to the
other; two corrupted slots give defaults; 600 consecutive saves across the
sequence-number wrap; user presets persist.

## 3. Emulator tests (`tests/emu_tests.py`, PyBoy 2.9, Game Boy Color mode)

**17 / 17 passed** (85 s wall time):

| Test | Result |
| --- | --- |
| Boot / title | screen matches the title picture (mean colour difference 3.86 / 255) |
| New city | walker placed on a walkable metatile |
| Streaming | 160 random walks (walking and running); 40 full comparisons of the hardware map (tile, VRAM bank, flips, palette for all 110 metatiles in view) against the host generator: **0 mismatches** |
| Collision | walker never on a solid metatile |
| World seam | save placed at (65530, 65533); walked across both x and y seams to (2, 9) with 0 map mismatches |
| Listening mode | walker held still for 8 s of input; music advanced 34 steps; walking resumes after SELECT |
| Synthesiser | DRONE 11 -> 10 and MELODY 10 -> 8 via the menu; 21 music steps elapsed with the menu open |
| Presets | all five factory presets load their values from the MEMORY page |
| Weather lock | HEAVY RAIN: rain level 15, 9 rain sprites on screen |
| Save write | SAVE PLACE writes a valid slot with the correct seed and position |
| Save restore | after a power cycle, CONTINUE restores seed and position |
| Corrupted save | newest slot damaged -> previous place restored, SAVE RECOVERED shown |
| Garbage SRAM | random SRAM ignored; a new city starts |
| Seed entry / determinism | typing 31337, visiting another city, and typing 31337 again gives the same start and identical streets |
| Audio | 90 s capture: sound in every second; all four channels active; spectrum keeps changing (mean change 0.93) |
| Long session | 30 emulated minutes of walking and listening: VBlank interrupt every frame, 7,736 music steps, lowest stack pointer 0xDFB3, final view matches the generator |

A quick variant (`--quick`, 5-minute session, 40 walks) also passes 17/17.

Earlier runs found and fixed: START ignored when pressed mid-step (now
remembered until the step ends); the synthesiser header clipped at the top of
the window; crowded rows on the WORLD and MEMORY pages; a ROM bank overflow
(renderer moved to bank 5); a weather-name pointer returned across banks; small
sound events rounding to zero volume; a drone level that masked the melody.

## 4. Audio analysis (capture of the ROM, tools/audio_preview)

A-weighted energy of a 2-minute walking capture: 400-4000 Hz (melody and pulse)
81%, below 150 Hz (drone) 10%. The spectrogram shows the drone gliding between
chord roots. `VACANT_Demo.wav` (180 s, 48 kHz stereo) was recorded from this
ROM with the EMPTY STREETS preset while walking a scripted route: 290 voice
triggers, 776 music steps.

## 5. Visual checks

`VACANT_Preview.png` is a contact sheet of emulator captures from this ROM:
title; Empty Streets in light rain; Industrial Quarter; Residential Towers in
fog; Lower City in heavy rain; Forgotten Places; a lightning flash; a train on
the elevated railway; the synthesiser. On a non-colour Game Boy (PyBoy DMG mode)
the ROM shows a notice that a Game Boy Color is required.

## 6. Clean rebuild

`tools/package_release.py` extracts `VACANT_Source.zip` into an empty
directory, builds there and runs the host tests. Result for this release:
see `build/clean_build.json` — the rebuilt ROM is byte-identical to the
release ROM (same SHA-256 as above) and all host tests pass.
