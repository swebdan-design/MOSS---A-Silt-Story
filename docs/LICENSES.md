# Licences

## MOSS itself

The repository's `LICENSE` file applies the **MIT licence** to the MOSS source
code and, as the earlier SILT releases did, to its original assets (pixel art,
maps, text, artwork, manual, liner notes and the soundtrack recordings).

**This was applied provisionally, to match SILT, and is the creator's
decision to confirm or change before public release.** What it means:

- Anyone may use, copy, modify, publish, distribute, sublicense and *sell*
  copies of the code **and** the art, writing and recordings, including in
  commercial products, provided the copyright notice and licence text are
  included.
- There is no warranty.
- It cannot be withdrawn from copies already distributed under it.

Common alternatives, if that is broader than intended:

| Option | Effect |
| --- | --- |
| MIT for code; **CC BY-NC-ND 4.0** for art, music and text | others may share the artistic work with credit, but not sell it or publish altered versions |
| MIT for code; **all rights reserved** for art, music and text | only the code is reusable; the album stays fully under the artist's control |
| **CC BY 4.0** for art, music and text | free reuse including commercial, with credit (closest to MIT for creative work) |

To change: edit `LICENSE`, add a `LICENSE-ASSETS` file if splitting, and
update this page and `release/MOSS_CREDITS.md`.

## Third-party components

| Component | Used for | Licence | Effect on MOSS |
| --- | --- | --- | --- |
| GBDK-2020 runtime library (crt0, `gb.lib`) | linked into the ROM | GPL v2 **with linking exception** (`licenses/LICENSE_GPLV2_LE` in GBDK) | the exception means the ROM is not covered by the GPL |
| SDCC 4.4.1 (compiler) | build tool | GPL v2+ (`LICENSE_SDCC`) | output is not covered |
| GBDK-2020 tools (lcc, makebin, romusage …) | build tools | per-tool licences in GBDK's `licenses/` | not distributed |
| SILT 5 × 7 alphabet | in-game font, artwork titles | MIT, © 2026 SILT contributors | notice kept in `assets/graphics/font.txt` |
| PyBoy 2.9.0 | tests | LGPL v3 | not distributed |
| SameBoy 1.0.1 core and boot ROMs | tests, soundtrack capture | MIT (SameBoy), boot ROMs MIT | not distributed; recordings are MOSS's own output |
| RGBDS 0.9.1 | building SameBoy's boot ROM | MIT | not distributed |
| Pillow, NumPy, ReportLab | artwork, analysis, manual | HPND / BSD-3 / BSD | not distributed |
| FFmpeg 6.1 | FLAC/MP3 encoding | LGPL/GPL | not distributed |
| DejaVu fonts | manual PDF body text (embedded) | Bitstream Vera / DejaVu licence (free, embedding allowed) | embedded subset in the PDF |

No Nintendo boot ROM, BIOS, logo artwork beyond the header bytes the hardware
requires, Pokémon, Studio Ghibli or other third-party game assets are used.
The cartridge header contains the standard Nintendo logo bitmap because the
DMG boot ROM refuses to start a cartridge without it; this is universal
practice for Game Boy homebrew.
