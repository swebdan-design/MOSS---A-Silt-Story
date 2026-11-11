# VACANT — Audio engine

Sources: `src/audio/sound.c` (hardware layer), `src/music/music.c` (composer),
both in bank 0. Tests: `tests/host_music.c`, emulator test `audio`.

## Hardware layer

| Function | Channel | Notes |
| --- | --- | --- |
| `snd_pulse` | CH1 / CH2 | MIDI note -> 11-bit period from a generated table; NRx2 envelope; duty; pan; CH1 sweep |
| `snd_pulse_period` | CH1 / CH2 | retune without retrigger (vibrato, pseudo-chords) |
| `snd_wave_start` | CH3 | loads one of five 16-byte timbres with the DAC off, then restarts |
| `snd_wave_period`, `snd_wave_level` | CH3 | glide and level (0, 25, 50, 100%) without restarting |
| `snd_noise` | CH4 | NR43 polynomial, envelope, pan |
| `snd_pan` | NR51 | per-channel left/right/centre |

Wave timbres (`tools/asset_generation/gen_audio_tables.py`): SOFT (sine), WARM
(three-harmonic saw), HOLLOW (odd harmonics), GLASS (fundamental plus bright
partials), BUZZ (soft narrow pulse).

## Timing

`music_tick()` is called by the VBlank interrupt at 59.73 Hz. A step lasts
`20 - SPEED` frames (0.33 s at SPEED 0, 0.084 s at 15). 16 steps make a bar,
4 bars a phrase. Per-frame work: channel hold timers, echo queue, drone
portamento and drift, melody vibrato, pseudo-chord switching, rain ticks.
Per-step work: composition. Nothing depends on the main loop.

## Channel arbitration

Each channel has a hold time and a priority. A role may start a sound only if
the channel is free or its priority is at least the current one:

| Priority | Users |
| --- | --- |
| 0 | rain ticks |
| 1 | melody echoes, wind, machinery, rain washes |
| 2 | melody, pulse part, electrical buzz |
| 3 | landmarks |
| 4 | train, thunder |

The drone owns CH3 permanently.

## Composition

- **Scales:** Aeolian, Dorian, Phrygian, minor pentatonic, whole tone,
  Suspended (0 2 5 7 9), Lydian, Locrian. `deg_semi` maps any scale degree to
  semitones across octaves.
- **Harmony:** chord root is a scale degree. Every HARM bars it moves by a
  weighted choice of 4ths/5ths/3rds (consonant), steps (more likely with
  TENSION), with a 27% pull back to the tonic. FREEZE HARMONY stops this.
- **Tonal centre:** ROOT + district offset (scaled by LOCATION) + macro drift of
  one fifth either way.
- **Drone:** chord root (or its fifth, 20%) in octave 2-3, glides between chords
  (1-4 period units per frame), detunes slowly by a triangle LFO (MOTION), and
  can breathe between two levels on alternate bars.
- **Melody:** four motifs of eight (degree, length) pairs, mostly stepwise with
  some thirds and rare leaps, about 20% rests. Statements follow phrase forms
  (A A B A, A B A C, …), are sometimes transposed to the current chord, and are
  separated by silences that shrink with DENSITY. In Forgotten Places the 2nd
  and 6th degrees are skipped (a pentatonic subset). TENSION adds occasional
  chromatic neighbours. Notes are capped below MIDI 91.
- **Echo:** 1-2 repeats on CH2 at half/quarter volume, 3 and 6 steps later,
  opposite pan.
- **Pulse textures:** arpeggio of chord tones; mechanism (euclidean hits on the
  chord root); shimmer (a pseudo-chord: CH2 retuned among three chord tones
  every two frames under one envelope); distant intervals with echo.
- **Noise:** rain ticks (probability = rain x NOISE), heavy-rain washes, wind,
  machinery (euclidean, industrial or mechanism style), thunder.
- **Breaths:** a phrase may be left to the drone and noise alone
  (probability rises as density falls).

## Evolution

| Scale | Every | Changes |
| --- | --- | --- |
| micro | note | velocity, duty, vibrato, chromatic neighbour, echo |
| meso | phrase | motif mutation (MUTATION), melody pan, breaths, register/key glide toward the district |
| macro | 2 + STABILITY/2 (+0-2) phrases | new motif, neighbouring mode and back, register drift, drone timbre, pulse texture, tonal centre a fifth away and back |

## Place and weather

District profiles move *targets*; the values in use glide one unit per bar:

| District | density | noise | register | melody | envelope | key | tension | texture |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Streets | 0 | 0 | 0 | 9 | 2 | 0 | 0 | preset |
| Industrial | +3 | +4 | -12 | 3 | 0 | +5 | +2 | mechanism |
| Residential | -1 | -2 | +12 | 11 | 2 | +3 | -2 | arpeggio |
| Lower City | -3 | 0 | -12 | 4 | 3 | +1 | +4 | intervals |
| Forgotten | -3 | -4 | +12 | 6 | 3 | +7 | -3 | preset, pentatonic |

Weather: rain raises noise and adds rain ticks; heavy rain darkens brightness by
one; fog lowers brightness and adds wind; clear lowers density by one; storm
adds tension and thunder gestures (CH4 low noise plus a falling CH1 sweep).

Landmarks: gain = 15 - Manhattan distance (0 beyond 15 metatiles), doubled in
focus mode. Focus also halves melody/pulse volumes and lowers the drone a level.

## Measured behaviour (host test, 10 simulated minutes each)

Every preset in every district produces melody (55-300 notes), pulse,
drone and (where appropriate) noise, with longest melodic rests of 15-60 s.
Eight simulated hours of the default preset produce 870-1040 melody notes per
hour, 50 mode changes and 50 tonal-centre changes. A district change causes no
drone restart. FREEZE HARMONY held the chord for ten minutes.

Emulator capture (A-weighted energy): 400-4000 Hz (melody, pulse) 81%,
below 150 Hz (drone) 10%.

## Capturing audio

```
python3 tools/audio_preview/capture_rom_audio.py out.wav 3 --walk --preset 0 --spectrogram spec.png
```
The WAV is the emulated Game Boy Color's own output (PyBoy, 48 kHz).
