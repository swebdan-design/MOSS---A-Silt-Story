# MOSS — Companion Soundtrack

*Synth Centric*

Ten recordings of the game's own music, captured from the ROM running in the
SameBoy emulator core (original DMG Game Boy model). Every sound is the Game
Boy's four channels as the game played them; nothing was re-synthesised,
layered or edited. Post-processing: DC removal, a 20 Hz high-pass (the
equivalent of the Game Boy's output capacitor), peak normalisation to
-1 dBFS, a 2-second fade-in and a 5-second fade-out. These are emulator
captures, **not** recordings of physical hardware.

This folder holds MP3 (LAME VBR quality 2, 48 kHz stereo). Lossless 48 kHz
16-bit FLAC and WAV masters are written by `tools/capture_soundtrack.py`
(about 120 MB, so they are not stored in the repository).

| # | Title | Length | Source in the game |
| --- | --- | --- | --- |
| 01 | The Cottage | 3:02 | Mae's cottage on the second day: the kitchen clock, a few pebble phrases. |
| 02 | Whispering Wood | 3:22 | The wood after wind, hollow wood and insects have been found. |
| 03 | The Three Stones | 3:13 | The creek: the three stones knocked deep, high, in between; then the water. |
| 04 | Summer Rain | 4:08 | The pond instrument, preset Summer Rain, left alone. |
| 05 | The Old Railway | 3:07 | The platform in the rain: the station bell and the cutting's echo, then absence. |
| 06 | The Hill | 3:15 | The hilltop in open fifths, crickets in the long grass. |
| 07 | The Pond at Evening | 4:08 | The pond instrument, preset In the Reeds. |
| 08 | Before Leaving | 4:08 | The pond instrument, preset Before Leaving. |
| 09 | The First Listener | 4:08 | The pond instrument, preset Quiet Clearing: stillness. |
| 10 | The Pond Remains | 6:08 | The pond instrument with SILT's original twelve-lily garden. |

Capture tool: `tools/capture_soundtrack.py`. Provenance per track: `provenance.json`.
