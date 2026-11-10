# Testing report

Date: 2026-11-10. ROM SHA-256 `d87219852f2153c2b9dc9b3a08f3c68b811ef0860125fbae970cd9fea239ac5e`.

Results below are copied from the machine-readable result files written by the
test scripts on this ROM. PASS means the check ran and passed; NOT TESTED
means it was not run.

## Summary

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


## Host tests

```
PASS: same seed -> identical 533 events
25 min: 6196 events  pebble 1738  living 1738  water 715  env 2005  notes 38..84
PASS: all four channels used
PASS: pitches inside table range (38..84)
PASS: density bounded (247 events/min)
steps without a pulse onset: 4860 of 7466 (65%)
PASS: the pond leaves silence (>=35% of steps)
PASS: no two 30-second pebble windows identical over 25 min (0 dups)
5 min events: active 601, still 377
PASS: Still produces markedly fewer events
preset 0 First Morning    : pebble   73 living   36 water   50 env   62
preset 1 The Creek        : pebble  132 living  146 water   66 env   67
preset 2 In the Reeds     : pebble   31 living  145 water   32 env   58
preset 3 Hollow Wood      : pebble   76 living    0 water   42 env  181
preset 4 Summer Rain      : pebble  145 living  160 water   36 env  353
preset 5 Quiet Clearing   : pebble   37 living   38 water   16 env   46
preset 6 Before Leaving   : pebble  147 living  166 water   64 env  155
preset 7 The Pond Remains : pebble   87 living   94 water   49 env   99
PASS: eight presets produce distinct channel signatures
PASS: interval memory learns repeated gesture (0 -> 8)
PASS: counter 0 bounded (0)
PASS: counter 1 bounded (0)
PASS: counter 2 bounded (0)
PASS: counter 3 bounded (239)
PASS: counter 4 bounded (0)
PASS: reset restores defaults
PASS: ripples cross lilies (142 crossings in 1 min)
PASS: stones at SILT cells
living+water events: no discoveries 0, all 50
PASS: first-day pond has no insects or water; discoveries add them

ALL HOST TESTS PASSED (0 failures)

sizeof(SaveData) = 184 bytes per copy
PASS: save fits its SRAM slot
PASS: blank SRAM -> no save, no crash
PASS: zeroed SRAM -> no save
PASS: write succeeds
PASS: round trip restores story and musical memory
PASS: newest of two alternating copies wins
PASS: damaged newest copy -> previous save recovered and reported
PASS: both copies damaged -> refused, reported
PASS: unknown version is not loaded
PASS: valid checksum but impossible values -> rejected
PASS: 20000 random/corrupted SRAM images never accepted invalid data
PASS: erase leaves no valid save

ALL SAVE TESTS PASSED (0 failures)
```

### Full playthrough — PyBoy

Emulator: pyboy. ROM: `MOSS_by_Synth_Centric.gb`.

| Check | Result | Detail |
| --- | --- | --- |
| boot to title | **PASS** | BGP=E4 |
| new game starts in the cottage on day 1 | **PASS** | (0, 4, 7, 1) |
| Mae introduced | **PASS** |  |
| village reached | **PASS** |  |
| pond discovered | **PASS** |  |
| first pebble answered from the reeds | **PASS** |  |
| notebook received from Mae | **PASS** |  |
| day 2 begins | **PASS** | 2 |
| met Iris | **PASS** |  |
| HOLLOW WOOD discovered | **PASS** | 0x8 |
| WIND discovered by listening in the reeds | **PASS** | 0x9 |
| THREE STONES puzzle -> WATER discovered | **PASS** | 0xb |
| INSECTS discovered in the meadow | **PASS** | 0xf |
| hill viewpoint | **PASS** |  |
| day 3 begins (rain) | **PASS** |  |
| RAIN SHELTER -> RAIN discovered | **PASS** | 0x1f |
| LAST BELL -> ECHO discovered | **PASS** | 0x3f |
| HIDDEN PATH revealed by listening | **PASS** |  |
| STILL CLEARING -> STILLNESS discovered | **PASS** | 0x7f |
| day 4 begins | **PASS** |  |
| wind chime -> MEMORY discovered | **PASS** | 0xff |
| all eight ways of listening found | **PASS** | 0xff |
| pond instrument entered | **PASS** |  |
| THE POND'S ANSWER | **PASS** |  |
| answer recorded in the story | **PASS** |  |
| day 5 begins (departure) | **PASS** |  |
| ending reached, stone left at the pond | **PASS** |  |
| post-game unlocked | **PASS** |  |
| Return to the pond opens the instrument | **PASS** |  |
| instrument keeps playing autonomously | **PASS** | 4558 |

30 of 30 passed.
194528 emulated frames (54 minutes of play).

### Full playthrough — SameBoy

Emulator: sameboy. ROM: `MOSS_by_Synth_Centric.gb`.

| Check | Result | Detail |
| --- | --- | --- |
| boot to title | **PASS** | BGP=E4 |
| new game starts in the cottage on day 1 | **PASS** | (0, 4, 7, 1) |
| Mae introduced | **PASS** |  |
| village reached | **PASS** |  |
| pond discovered | **PASS** |  |
| first pebble answered from the reeds | **PASS** |  |
| notebook received from Mae | **PASS** |  |
| day 2 begins | **PASS** | 2 |
| met Iris | **PASS** |  |
| HOLLOW WOOD discovered | **PASS** | 0x8 |
| WIND discovered by listening in the reeds | **PASS** | 0x9 |
| THREE STONES puzzle -> WATER discovered | **PASS** | 0xb |
| INSECTS discovered in the meadow | **PASS** | 0xf |
| hill viewpoint | **PASS** |  |
| day 3 begins (rain) | **PASS** |  |
| RAIN SHELTER -> RAIN discovered | **PASS** | 0x1f |
| LAST BELL -> ECHO discovered | **PASS** | 0x3f |
| HIDDEN PATH revealed by listening | **PASS** |  |
| STILL CLEARING -> STILLNESS discovered | **PASS** | 0x7f |
| day 4 begins | **PASS** |  |
| wind chime -> MEMORY discovered | **PASS** | 0xff |
| all eight ways of listening found | **PASS** | 0xff |
| pond instrument entered | **PASS** |  |
| THE POND'S ANSWER | **PASS** |  |
| answer recorded in the story | **PASS** |  |
| day 5 begins (departure) | **PASS** |  |
| ending reached, stone left at the pond | **PASS** |  |
| post-game unlocked | **PASS** |  |
| Return to the pond opens the instrument | **PASS** |  |
| instrument keeps playing autonomously | **PASS** | 4571 |

30 of 30 passed.
195384 emulated frames (55 minutes of play).

### System tests — PyBoy

Emulator: PyBoy. ROM: `MOSS_by_Synth_Centric.gb`.

| Check | Result | Detail |
| --- | --- | --- |
| blank 0xFF SRAM boots to title without a save | **PASS** | status=0 |
| zeroed SRAM boots to title without a save | **PASS** | status=0 |
| 12 random-garbage SRAM images: title still appears | **PASS** |  |
| Continue restores day 2 in the village | **PASS** | (1, 7, 7, 0) |
| saved position survives a power cycle | **PASS** | (1, 9, 7, 2) |
| damaged newest save -> recovered from the other copy | **PASS** | status=2 |
| player is told about the recovery | **PASS** |  |
| recovered save is playable | **PASS** | (1, 7, 7, 0) |
| both copies damaged -> reported, no crash | **PASS** |  |
| player is told the notebook could not be read | **PASS** |  |
| new summer still starts after corruption | **PASS** | (0, 4, 7, 1) |
| 20 cycles of sitting at the pond and standing up | **PASS** |  |
| no audio queue leak after the cycles | **PASS** | dropped=1 |
| 25 minutes of autonomous pond playback | **PASS** | 1500 s captured |
| pond keeps sounding (events dispatched throughout) | **PASS** | 2659 hardware events (16-bit counter) |
| activity present in every quarter of the session | **PASS** | [np.float32(3.02), np.float32(2.89), np.float32(3.03), np.float32(3.06)] |
| the pond breathes: quiet and fuller passages (RMS p10/p90) | **PASS** | 2.28/4.23 |
| no repeated 30-second passage (max envelope correlation < 0.9) | **PASS** | 50 windows, max r=0.58 |

18 of 18 passed.


## What the playthrough does

It boots the ROM, starts a new summer and plays all five days with ordinary
button presses, path-finding on the live collision map read from RAM: meets
everyone needed, drops the first pebble, receives the notebook, finds all
eight ways of listening, solves the Three Stones, Rain Shelter, Last Bell,
Hidden Path, Still Clearing and Pond's Answer, sleeps through each night,
watches the whole ending, and then opens Return to the Pond from the new
title menu and changes a preset. Screenshots of every step are written to
`build/qa*/`.

## Audio checks

The 25-minute autonomous session (system tests) records the emulator's audio,
checks that sound continues throughout each quarter, that loudness varies
(quiet and fuller passages), and that no 30-second passage repeats (envelope
correlation). Host tests check determinism, channel use, pitch bounds,
density control, silence (about two thirds of steps without a new pulse
note) and that no two 30-second melody windows match over 25 minutes.

## Visual checks

Screens were reviewed by eye from the playthrough screenshots: text wrapping,
notebook pages, menus (including the 14-item scrolling pond menu), pond
ripples and lily flashes, the ending, day palettes. Problems found and fixed
during development included: sprites over the text box, a menu buffer overflow
in the post-game pond menu, tile animation overwriting text on full-screen
pages, a VBlank-length audio interrupt that stalled the LCD-off wait, and a
wrong warp target. Sprite-per-line limits are handled by design (see
KNOWN_LIMITATIONS); no flicker multiplexing is used.
