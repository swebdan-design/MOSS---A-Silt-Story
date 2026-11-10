# Known limitations

Stated plainly, so nothing here is mistaken for a finished claim.

## Testing

- **Not tested on physical Game Boy hardware or a flash cartridge.** All
  testing used two emulators (PyBoy 2.9.0 and the SameBoy 1.0.1 core in DMG
  mode). SameBoy is cycle-accurate, including VRAM access timing, which is the
  strongest evidence available here — but it is not hardware.
- Not tested in BGB, Emulicious, mGBA or Gambatte.
- Playtime has not been measured with people. The automated playthrough,
  which takes direct routes and reads every line at machine speed, takes about
  54 emulated minutes including all waits. A first human playthrough is
  estimated at 60–120 minutes. The brief's 90–150 minute target may not be met
  by a quick player.

## Audio

- Hard stereo only (the DMG pans each channel fully left, right or centre).
- Restarting the wave channel switches its DAC off and on to avoid the DMG
  wave-RAM corruption bug. When the water drone is off, a Root lily can make a
  very small click at its start.
- The composer runs in the main loop with up to 8 ticks (≈130 ms) of
  look-ahead. A main-loop stall longer than that (for example a map load
  with the LCD off) can delay the next step slightly; missed steps are
  skipped, never burst.
- Two sounds cannot share a channel. When the queue or a channel is busy, the
  less important event is dropped (counted in `audio_dropped`).
- No in-ROM recording. The soundtrack was captured externally from an
  emulator (see the soundtrack provenance file).
- PyBoy's audio output is coarse (8-bit); SameBoy was used for recordings.

## Graphics

- The DMG shows at most 10 sprites per scanline. Ripple rings use up to 8
  sprites each and are thinned to 4 points when more than four rings are
  active; on lines where hardware would exceed 10, some ring points are simply
  not shown for that line (no flicker multiplexing).
- Non-player characters glide when they walk (they have one frame per
  direction); only the boy and the cat have walking frames.
- Rendering text (variable-width) to tiles costs CPU; menus that scroll can
  miss a very fast second key press while redrawing.

## Design simplifications against the brief

- The pond grid keeps SILT's 8 × 5 cells but wave speed and lifetime are
  scaled to 16-pixel cells; at most six waves (SILT: eight).
- The Rain Shelter and Still Clearing puzzles are single listening moments,
  not multi-step puzzles. The Pond's Answer accepts one gesture (low row, then
  second row). Multiple solutions exist for the Hidden Path (listening or the
  cat) and the Three Stones (sound or ring sizes).
- Post-game "free exploration" resumes the last afternoon (day 4); the
  departure is not replayed.
- Music memory keeps pitch-row and behaviour statistics that are saved and
  documented but only interval, rhythm, voice, scale and density currently
  influence the music.

## Packaging

- SILT III's release binaries and cover art were not available, so the
  continuity audit could not inspect them.
- The licence (MIT, matching SILT) was applied provisionally; see LICENSES.md.
