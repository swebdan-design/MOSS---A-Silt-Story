# VACANT — Known limitations

## Not yet verified

- **Physical hardware.** VACANT has been tested only in an emulator (PyBoy 2.9,
  Game Boy Color mode). It has not been run on a physical Game Boy Color, a
  Game Boy Advance, or any flash cartridge. It is written for the hardware (no
  emulator-only behaviour, VRAM writes wait for accessible LCD modes, palette
  and sound registers are written in VBlank), but real-hardware timing,
  double-speed behaviour and SRAM saving remain unverified.
- **Second emulator.** Only PyBoy was used. Results in SameBoy, BGB, Emulicious,
  mGBA or Gambatte are expected to match but were not checked.
- **Flash cartridges.** Saves need MBC5 battery-RAM support and a cartridge that
  writes save RAM back to its storage.

## Design limits

- **Four voices.** Echoes, landmarks, trains and thunder borrow channels by
  priority, briefly interrupting the part that normally uses them. No reverb and
  no polyphony beyond the hardware.
- **Wave timbre changes** restart the wave channel (on phrase boundaries), which
  can produce a small click. Changing presets restarts the drone.
- **District colour** is a single wall colour for the whole screen, gliding to the
  district you stand in; a neighbouring district is shown in that colour until
  you cross.
- **Rain** uses at most ten 8 x 16 sprites; **fog** is a palette change rather
  than a moving layer.
- **Trains** move in 8-pixel steps (they are drawn into the background map).
- **Seeds** are 16-bit (65,536 cities).
- **World size:** 65,536 x 65,536 metatiles; positions wrap seamlessly at the
  edges (about five hours of straight walking in one direction).
- **Repairs.** About 3% of blocks contain a pocket the layout rules enclosed;
  it is filled with a bush so it can never trap the walker.
- **Interface text** is upper case only.
- The synthesiser's World page saves one place; presets have four user slots.

## Known quirks

- In PyBoy the top 3-4 pixel rows of the window layer were not shown when the
  window starts at line 0; the synthesiser leaves its first row empty so nothing
  is lost. Whether real hardware shows those rows was not checked.
- The district name popup appears when you cross into a district; walking along
  a boundary can show it several times.
