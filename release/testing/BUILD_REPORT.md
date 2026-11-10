# Build report

Date: 2026-11-10

| | |
| --- | --- |
| ROM | `MOSS_by_Synth_Centric.gb` |
| SHA-256 | `d87219852f2153c2b9dc9b3a08f3c68b811ef0860125fbae970cd9fea239ac5e` |
| Size | 256 KiB |
| Toolchain | GBDK-2020 4.3.0 (SDCC : z80/sm83/mos6502/mos65c02 TD- 4.4.1 #14650 (Linux)) |
| Host | Python 3.13.18, cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| Reproducible | yes — a clean checkout of the same commit rebuilt to the identical SHA-256 |

## Header verification

```
PASS Nintendo logo bitmap present (required by the boot ROM)
PASS title "MOSS"
PASS CGB flag 0x00: DMG game, no Game Boy Color features required
PASS SGB flag 0x00
PASS cartridge type 0x1B (MBC5+RAM+BATTERY)
PASS ROM size code 3 = 256 KiB, file 256 KiB
PASS RAM size code 2 (8 KiB)
PASS header checksum 0xBB
PASS global checksum 0xF719
sha256 d87219852f2153c2b9dc9b3a08f3c68b811ef0860125fbae970cd9fea239ac5e
```

## ROM bank usage (romusage)

```

Bank         Range                Size     Used  Used%     Free  Free% 
--------     ----------------  -------  -------  -----  -------  -----
ROM_0        0x0000 -> 0x3FFF    16384    13326    81%     3058    19% |-######################-....|
ROM_1        0x4000 -> 0x7FFF    16384     8910    54%     7474    46% |###############.............|
ROM_2        0x4000 -> 0x7FFF    16384     6044    37%    10340    63% |##########-.................|
ROM_3        0x4000 -> 0x7FFF    16384    15998    98%      386     2% |###########################-|
ROM_4        0x4000 -> 0x7FFF    16384     8797    54%     7587    46% |###############.............|
ROM_10       0x4000 -> 0x7FFF    16384     3081    19%    13303    81% |#####-......................|
ROM_11       0x4000 -> 0x7FFF    16384     6641    41%     9743    59% |###########-................|
ROM_12       0x4000 -> 0x7FFF    16384    15492    95%      892     5% |##########################-.|
ROM_13       0x4000 -> 0x7FFF    16384     4211    26%    12173    74% |#######.....................|
ROM_14       0x4000 -> 0x7FFF    16384     1826    11%    14558    89% |###.........................|
ROM_15       0x4000 -> 0x7FFF    16384     5850    36%    10534    64% |##########..................|
WRAM_LO      0xC000 -> 0xCFFF     4096     3189    78%      907    22% |.-####################-.....|

```

Banks: 0 home (frame loop, timer-driven audio, VRAM, text, saves); 1 composer,
garden, memory; 2 overworld; 3–4 generated tiles, maps, sprites, pictures;
10 story flow; 11 people; 12 places and puzzles; 13 notebook and menus;
14 title and ending; 15 pond instrument. WRAM: about 3.2 of 4 KiB in the
first bank is used by data; the stack lives in the second WRAM bank.
