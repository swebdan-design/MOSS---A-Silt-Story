# VACANT — Credits and licences

**VACANT — An Endless City After Dark**
by **Synth Centric**

## Original work

Concept, program code, procedural city generator, generative music system,
pixel art (tiles, metatiles, sprites, font, title picture), album artwork,
manual and documentation were created for this release. All artwork was drawn
procedurally by the painters in `tools/asset_generation/`; no images, sounds or
code from commercial games or other artists were used. The demonstration
recording is the ROM's own output, captured from an emulator; nothing was added
or replaced on a computer.

Signage in the city ("REST IS OPTIONAL", "MEMORY SERVICES", "PUBLIC QUIET
ZONE", "STAY AVAILABLE", "VACANCY" and others) is original architectural
decoration.

## Licence

VACANT's source code and original assets are released under the **MIT licence**
(`LICENSE`), matching earlier Synth Centric releases. This was applied
provisionally. MIT allows anyone to use, modify, distribute and sell copies of
both the code and the artwork, with the copyright notice. If that is broader
than intended for the artwork, music and writing, common alternatives are MIT
for code with CC BY-NC-ND 4.0 or "all rights reserved" for the creative work;
change `LICENSE` and this page before public release.

## Third-party components

| Component | Used for | Licence | Effect on VACANT |
| --- | --- | --- | --- |
| GBDK-2020 4.5.0 runtime (crt0, libraries) | linked into the ROM | GPL v2 with linking exception | the exception means the ROM is not covered by the GPL |
| SDCC 4.5.1 | compiler | GPL v2+ | compiler output is not covered |
| GBDK-2020 tools (lcc, makebin, romusage) | build | per-tool licences in GBDK's `licenses/` | not distributed |
| Pillow | asset compiler, artwork | MIT-CMU (HPND) | build tool only |
| PyBoy 2.9 | emulator tests, audio capture, screenshots | LGPL v3 | not distributed |
| NumPy | test analysis | BSD | not distributed |
| ReportLab | manual PDF | BSD | not distributed |

GBDK-2020 is not included in the source package; `tools/bootstrap_toolchain.sh`
downloads the official release.

"Game Boy" and "Game Boy Color" are trademarks of Nintendo. VACANT is an
independent homebrew work, not affiliated with or endorsed by Nintendo. The
cartridge header contains the boot logo bytes the hardware requires to start
any cartridge.
