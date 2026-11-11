# VACANT

**An Endless City After Dark** — by **Synth Centric**

![VACANT album cover](VACANT_Album_Cover.png)

An entire futuristic city inside a Game Boy Color cartridge. It is always
night. The streets are empty. You can walk them, listen, adjust the music, and
stay as long as you like. There is no story, no goal and no ending.

The city is generated from a seed, block by block, as you walk: empty streets
with neon signs and blinking signals, an industrial quarter of factories and
substations, residential towers, a lower city of canals and abandoned
platforms, and forgotten parks and ruins. The same seed always builds the same
city. The soundtrack is composed in real time by the console's four sound
channels and is shaped by where you are, by the weather and by a small
synthesiser you can open at any time.

The ROM is the album. The city is the instrument.

## In this release

| File | What it is |
| --- | --- |
| `VACANT_by_Synth_Centric.gbc` | the cartridge image (128 KiB, MBC5 + RAM + battery, Game Boy Color only) |
| `VACANT_Manual.md` / `.pdf` | the manual |
| `VACANT_Album_Cover.png` | album artwork, 3000 x 3000 |
| `VACANT_Preview.png` | screenshots captured from the running ROM |
| `VACANT_Demo.wav` | three minutes of the ROM's own output, recorded from an emulator |
| `VACANT_Source.zip` | complete source, assets, tools and tests |
| `CONTROLS.md`, `BUILD_INSTRUCTIONS.md`, `TESTING_REPORT.md`, `KNOWN_LIMITATIONS.md`, `CREDITS_AND_LICENSES.md` | reference |
| `VACANT_Complete_Release.zip` | all of the above in one archive |

## Play

- **Emulator:** open the `.gbc` file in a Game Boy Color emulator (SameBoy, BGB,
  Emulicious, mGBA, Gambatte …). Saves go to the emulator's battery file.
- **Game Boy Color:** copy the `.gbc` to a flash cartridge with MBC5 and
  battery-backed save support. Physical hardware has not yet been tested (see
  `KNOWN_LIMITATIONS.md`).

## Controls

D-pad walk · B hold to walk faster · A ambient focus · START synthesiser ·
SELECT listening mode. Details in `CONTROLS.md` and the manual.

## Build

```bash
sh tools/bootstrap_toolchain.sh
```

```bash
make
```

See `BUILD_INSTRUCTIONS.md`.
