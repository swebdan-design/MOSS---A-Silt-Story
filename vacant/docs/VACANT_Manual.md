# VACANT

## An Endless City After Dark

### by Synth Centric

---

## 1. Introduction

VACANT is a city inside a Game Boy Color cartridge.

It is night there, and it stays night. The streets are empty. The windows are lit. Rain comes and goes. An elevated train crosses overhead now and then and is gone. Nobody needs anything from you, and there is nowhere you are supposed to be.

You can walk. You can listen. You can stand still for as long as you like.

The city is generated from a number, a *seed*. The same seed always builds the same streets, the same towers, the same quiet courtyards. A different seed builds a different city. Each one is far larger than anyone will walk.

The music is generated too. It is not a recording and not a set of loops: it is composed as you hear it, by the Game Boy Color's own four sound channels, and it never needs to repeat in exactly the same way. The part of the city you are standing in shapes it. So does the weather. So do you, through a small synthesiser you can open at any moment without the music stopping.

VACANT has no story, no goals, no score and no ending. Think of it as an ambient record you walk through.

---

## 2. Installation

**On a Game Boy Color.** VACANT is a 128 KiB cartridge image (`VACANT_by_Synth_Centric.gbc`) for an MBC5 cartridge with 8 KiB of battery-backed save RAM. Copy it to a flash cartridge that supports MBC5 with battery saves, following that cartridge's instructions. Saves only persist if the cartridge (or its menu) writes the save RAM back to storage; check how yours handles `.sav` files.

**In an emulator.** Open the `.gbc` file in any accurate Game Boy Color emulator (for example SameBoy, BGB, Emulicious, mGBA or Gambatte). Select Game Boy Color mode if the emulator asks. Your saves go into the emulator's battery file (usually `.sav`); keep it beside the ROM.

VACANT needs a Game Boy Color (or a Game Boy Advance, which can run Game Boy Color cartridges). On an original Game Boy or Game Boy Pocket it shows a short message instead.

---

## 3. Controls

| Button | While walking | In the synthesiser |
| --- | --- | --- |
| **D-pad** | walk | **Up/Down** choose a setting, **Left/Right** change it |
| **B** | hold to walk faster | close the synthesiser (or cancel seed entry) |
| **A** | ambient focus on/off | confirm, toggle or perform the chosen action |
| **START** | open the synthesiser | close the synthesiser |
| **SELECT** | Listening Mode on/off | next page |

On the title screen press **START**. If a saved place exists you can choose **CONTINUE** or **NEW CITY** (Left/Right, then A or START).

---

## 4. Exploring the city

You walk one paving stone at a time, as in a classic top-down Game Boy game. Hold **B** to walk twice as fast.

Streets run unbroken from north to south through the whole city; every block has a pavement around it, and every pavement leads back to a street. Buildings, water, fences, railway decks and street furniture block your way; pavements, roads, alleys, plazas, grass and shallow water do not.

The city is divided into five kinds of place. When you cross into one, its name appears briefly at the foot of the screen, and the city's light changes slowly to match.

- **EMPTY STREETS** — long roads, rain-dark pavements, crosswalks, blinking traffic signals at empty junctions, vending machines, utility poles, shopfronts and neon signs.
- **INDUSTRIAL QUARTER** — factory halls with sawtooth roofs, chimneys and ventilation, storage yards of crates, drums and containers, rail lines set into the roads, fenced power substations with red warning lights.
- **RESIDENTIAL TOWERS** — tall blocks with balconies and patterns of lit windows, rooftops crowded with fans, water tanks and antennae, gardens and narrow passages, streets lined with trees.
- **LOWER CITY** — dark paving, drainage canals with grated crossings, concrete bunkers wrapped in pipes, stairwells going down, abandoned railway platforms.
- **FORGOTTEN PLACES** — empty parks, overgrown courtyards with dry fountains, flooded walkways, ruined buildings with vines, lamps that no longer light.

Districts sit next to one another the way neighbourhoods do: parks give way to housing, housing to the main streets, the streets to industry, and the lower city lies beneath the busier districts.

Small things happen without you. Streetlamps fail and flicker. Signs die and stutter back to life with a buzz. Rooftop fans turn. Trees lean in the wind. Water moves. Warning lights pulse on substations and roofs. Traffic signals blink for no one. Now and then a train crosses the elevated railway, its windows lit; if you are standing beneath the deck you will be hidden under it as it passes.

---

## 5. Procedural generation

Nothing in VACANT's city is stored. Every paving stone is calculated from the seed and its own coordinates, at the moment it comes onto the screen:

1. **Street lines.** The city is laid out in squares of 64 × 64 paving stones. Inside each, four street lines run in each direction, nudged a little from a regular grid by the seed, so blocks vary from 12 to 20 stones across.
2. **Districts.** A slow, smooth noise field across the city decides each block's district, which is why districts form natural neighbourhoods rather than a patchwork.
3. **Roads.** Each block owns the street on its west side and the one on its north side. North–south streets are always open. East–west streets can become canals, pedestrian plazas, roads with rail lines, or run beneath an elevated railway, depending on district.
4. **Lots.** The inside of a block is divided by alleys into up to four lots, and each lot becomes an archetype suited to its district: a shop with a sign, an apartment block, a factory, a yard, a tower, a garden, a bunker, a drainage channel, a park, a ruin.
5. **Details.** Roof equipment, window lights, shopfronts, signs, street furniture and planting are chosen within each archetype.
6. **Landmarks.** About one block in five holds an acoustic landmark (section 7).
7. **A check.** Finally the block is searched outward from its streets; if any open ground has been enclosed by mistake, it is planted over, so you can never be trapped.

Because each block depends only on the seed and its position, walking in a different order, or coming back hours later, always rebuilds exactly the same place.

**How big is it?** The city is 65,536 paving stones in each direction — more than a million pixels across. It wraps around at its edges like a globe, seamlessly, so there is no wall and no end. Walking straight across it takes about five hours at walking pace.

---

## 6. World seeds

Every city has a seed between **00000** and **65535**.

- **NEW CITY** (WORLD page) builds a city from a new random seed.
- **SEED** (WORLD page): press **A** to edit, then **Left/Right** to choose a digit and **Up/Down** to change it. Press **A** again to go to that city, or **B** to cancel.
- The city appears behind a short fade, and you begin on a street in the Empty Streets nearest its centre.

Write down seeds you like. Anyone with VACANT who types the same number will walk the same city.

---

## 7. Acoustic districts

Each district has its own acoustic personality, and the music leans toward it while you are there:

| District | What changes in the music |
| --- | --- |
| Empty Streets | deep drone, sparse electronic melody, soft noise, wide spacing |
| Industrial Quarter | lower register, mechanical pulses and metallic noise in repeating patterns, little melody |
| Residential Towers | higher, gentler register, soft arpeggios, consonant harmony |
| Lower City | very low and slow, long intervals between events, distant intervals, more unsettled harmony |
| Forgotten Places | quiet, sparse pentatonic fragments, almost no rhythm, a warmer tonal centre |

The music is never restarted when you cross a boundary. Instead the values it plays with — density, register, noise, envelope length, how often the melody speaks, its preferred key — glide toward the new district one small step at a time, over tens of seconds.

**Landmarks.** Some places sound on their own. As you approach, they grow louder; walk away and they fade:

- a **neon tower** on the streets: an electrical buzz in short clusters;
- a **power substation**: a regular low-frequency pulse;
- **rooftop vents** above the residential towers: a slow mechanical breathing;
- an **abandoned platform** in the lower city: a distant repeated interval and a faint electrical hum;
- a **quiet garden** among the forgotten places: the noise of the city falls away and small high bells appear.

**Ambient focus.** Press **A** while walking. The city dims slightly, the main voices step back, and nearby landmarks come forward. Press **A** again to return.

---

## 8. The four-channel synthesiser

The Game Boy Color has four sound channels, and VACANT uses exactly those:

| Channel | Hardware | Musical role |
| --- | --- | --- |
| 1 | pulse wave with pitch sweep | **MELODY**, high bells near gardens, thunder and train gestures |
| 2 | pulse wave | **PULSE** (arpeggios, mechanisms, shimmering chords, intervals) and the **echoes** of the melody |
| 3 | programmable wave | **DRONE**: a sustained low voice that glides between harmonies |
| 4 | noise | **NOISE**: rain, wind, machinery, thunder, electrical buzz |

There are only four real voices, so roles share. An echo of the melody can only sound on channel 2 when the pulse part is resting; a landmark or a passing train may borrow a channel for a moment, by priority, and then hand it back. The MIX controls set how loud each *role* is, not each chip channel.

Some of the sounds in VACANT come straight from the hardware: chords are made by switching one channel rapidly between three notes; echoes are quieter repeats placed in the opposite ear; the drone slides between notes and drifts very slightly out of tune for warmth; rain is a scatter of tiny noise bursts panned left and right. There is no reverb and no sampled sound: depth comes from spacing, register and repetition.

---

## 9. Generative composition

The composer keeps time in *steps*, grouped into bars of sixteen and phrases of four bars. It runs from the Game Boy's display interrupt, about sixty times a second, so it never waits for the graphics and never depends on how fast you walk.

- **Harmony.** There is a tonal centre (the ROOT), a mode, and a current chord built on one of the mode's degrees. Every few bars the chord moves — mostly by fourths, fifths and thirds, with a pull back toward home — unless harmony is frozen.
- **Melody.** The melody is built from four short motifs of eight notes. A phrase states a motif, rests, then states another, in forms such as A A B A. Motifs are sometimes sung relative to the current chord, so the same shape takes new colours.
- **Silence.** Every statement is followed by space, and now and then a whole phrase is left to the drone alone.

The music changes on three time-scales:

- **Micro** (every note): loudness, tone colour (pulse width), length, vibrato.
- **Meso** (every phrase): which motif is heard, mutations to the motifs, stereo placement, breathing spaces.
- **Macro** (every few phrases, set by STABILITY): a new motif, a shift to a neighbouring mode, a change of register or drone timbre, a different pulse texture, the tonal centre drifting a fifth away and back.

Some things stay recognisable — the presets' scales, the drone, the motifs you have heard — while others slowly turn over.

---

## 10. Musical parameters

Open the synthesiser with **START**. The music carries on while you change things. The bottom line shows what the music is doing now: the current key, the mode, and the chord as a Roman numeral (a dash appears during a breathing space).

**Page 1 — MIX**

| Setting | Range | Effect |
| --- | --- | --- |
| DRONE | 0–15 | level of the wave-channel drone (0 = off) |
| MELODY | 0–15 | loudness of the melody |
| PULSE | 0–15 | loudness of the pulse part |
| NOISE | 0–15 | loudness of rain, wind and machinery |

**Page 2 — TONE**

| Setting | Range | Effect |
| --- | --- | --- |
| ROOT | C … B | the home key |
| MODE | 8 modes | AEOLIAN (natural minor), DORIAN, PHRYGIAN, PENTATONIC (minor), WHOLE TONE, SUSPENDED (an open ambient set), LYDIAN, LOCRIAN |
| BRIGHTNESS | 0–15 | register and tone colour: low values are lower and rounder, high values higher and thinner |
| TENSION | 0–15 | how far harmony strays from home, and how often the melody leans on a chromatic neighbour |

**Page 3 — EVOLVE**

| Setting | Range | Effect |
| --- | --- | --- |
| DENSITY | 0–15 | how many events happen, and how short the silences are |
| MUTATION | 0–15 | how quickly motifs are altered |
| SPEED | 0–15 | the length of a step: from about 0.33 s (0) to 0.08 s (15) |
| STABILITY | 0–15 | how long the music stays itself before a large change |

**Page 4 — WORLD**

| Setting | Effect |
| --- | --- |
| SEED | the current city's number; **A** to edit and travel (section 6) |
| WEATHER | AUTO, or lock CLEAR, LIGHT RAIN, HEAVY RAIN, FOG or STORM |
| ATMOSPHERE (0–15) | how restless the city is: flickering lights, failing signs, passing trains, how often the weather turns |
| LOCATION (0–15) | how strongly the district you are in shapes the music (0 = not at all) |
| NEW CITY | travel to a random seed |
| SAVE PLACE | save the city, your position and the synthesiser |
| RETURN TO SAVE | go back to the saved place |

**Page 5 — MEMORY**

| Setting | Effect |
| --- | --- |
| SAVE PRESET | **Left/Right** choose slot 1–4, **A** stores the current music (a dot marks a used slot) |
| LOAD PRESET | **Left/Right** choose one of five factory presets or user slots 1–4, **A** loads it |
| FREEZE HARMONY | **A** holds the current chord and key until released |
| RANDOMISE MUSIC | **A** creates a new, coherent configuration with new motifs |
| RESTORE DEFAULTS | **A** returns to EMPTY STREETS with automatic weather |

Presets also carry settings that are not on the pages: the pulse texture, the drone's timbre, how many bars each chord lasts, the number of echoes and the depth of vibrato and drift.

---

## 11. Presets

Five musical configurations are built in. They are not levels; they are ways of hearing the same city.

| Preset | Key and mode | Character |
| --- | --- | --- |
| **EMPTY STREETS** | A, Aeolian | the default: a soft sine drone, slow arpeggios, sparse melody with one echo |
| **THE WORKS** | E, Phrygian | dense and mechanical: a buzzing drone, euclidean pulses, metallic noise, long chords |
| **LOWER CITY** | C#, Locrian | dark and slow, very sparse, distant intervals answered by two echoes, deep vibrato |
| **NIGHT SERVICE** | D, Dorian | brighter and busier: shimmering pseudo-chords, quick harmonic movement |
| **BEFORE DAWN** | F, Lydian | high and open, glassy drone, gentle arpeggios, low tension, frequent change |

Each preset includes its own musical seed, so loading it always begins with the same motifs. RANDOMISE MUSIC chooses values inside ranges that sound coherent (for example, unsettled modes are given more tension, bright modes more brightness, and mechanical textures a faster pulse).

---

## 12. Listening Mode

Press **SELECT** to stop. The walker stands still and faces you; the word LISTENING appears for a moment and then the screen is left to the city. Lights flicker, rain falls, trains pass and the music keeps evolving, for as long as you leave it. There is no timer and nothing will interrupt you. Press **SELECT** again to walk on.

---

## 13. Weather

| Weather | What you see | What you hear |
| --- | --- | --- |
| CLEAR NIGHT | dry, sharp night colours | sparser, more open music |
| LIGHT RAIN | a few rain streaks, puddles ripple | a scatter of soft rain ticks |
| HEAVY RAIN | dense rain and splashes, the city darkens, wet ground shines | many rain ticks and washes of noise, slightly darker tone |
| FOG | a violet haze lifts the shadows and softens the lights | less brightness, more wind |
| DISTANT STORM | rain, and sheet lightning across the sky | thunder after the lightning, low falling gestures, a little more tension |

With WEATHER set to AUTO the sky changes by itself every few minutes, moving between neighbouring states (clear to light rain, light rain to heavy rain or fog, heavy rain to storm, and back). All changes are gradual: rain and fog thicken or thin over several seconds. Set WEATHER to a fixed state to keep it.

---

## 14. Saving and loading

VACANT does not save the city — it does not need to. It saves the seed, and rebuilds the city exactly.

**SAVE PLACE** stores: the city's seed, your position and direction, the musical seed, every synthesiser setting, the weather setting and whether harmony is frozen. **SAVE PRESET** stores up to four of your own musical configurations.

Next time, choose **CONTINUE** on the title screen, or **RETURN TO SAVE** on the WORLD page.

The cartridge keeps two copies of the save. Each new save is written over the older copy and verified, and each copy carries a checksum. If a copy is damaged — for example by power failing during a write — VACANT uses the other one and shows **SAVE RECOVERED**. If neither is valid, or the cartridge is new, VACANT simply starts a fresh city.

On a flash cartridge, saves only survive if the cartridge stores its save RAM (see section 2).

---

## 15. Hardware compatibility

- **System:** Game Boy Color. The cartridge header marks VACANT as Game Boy Color only; an original Game Boy shows a message instead of running it. A Game Boy Advance runs Game Boy Color software and should work, but has not been tested.
- **Cartridge:** MBC5 with RAM and battery (type 0x1B), 128 KiB ROM, 8 KiB SRAM.
- **CPU:** VACANT runs the Game Boy Color in double-speed mode.
- **Emulators:** tested in PyBoy 2.9 (Game Boy Color mode). Any accurate Game Boy Color emulator should run it.
- **Physical hardware:** at the time of release VACANT has **not** been tested on a physical Game Boy Color or flash cartridge. It is written for the hardware (it uses no emulator-only behaviour, waits for safe video timing, writes its sound and palette registers during the vertical blank), but this has not been verified on a real console.

---

## 16. Technical limitations

- **Four voices.** The music has four real channels. Echoes, landmarks, trains and thunder borrow them; while one is borrowed, its usual part pauses. There is no true reverb or polyphony beyond the hardware.
- **Wave timbre changes** need the wave channel to restart, which happens only on a phrase boundary and can make a small click.
- **District colour** is one colour for all buildings on screen. When you stand at a boundary, the neighbouring district is shown in the colour of the one you are in until you cross.
- **Rain** is drawn with at most ten small sprites, so it is suggestive rather than dense. Fog is a change of colour, not a moving layer.
- **Trains** are drawn by rewriting the background in steps of eight pixels, so they move in small jumps.
- **The world wraps** after 65,536 paving stones in each direction (about five hours of straight walking). There is no edge.
- **Seeds** are five-digit numbers (0–65535), so there are 65,536 cities.
- **Physical hardware** testing is still to be done (section 15).

---

## 17. Credits and licences

**VACANT — An Endless City After Dark** by **Synth Centric**.

Concept, code, pixel art, generative music system, album artwork and this manual are original to this release. No graphics, sound or code from commercial games were used. The album cover and all in-game art were drawn procedurally by the tools in the source package; the demonstration recording is the ROM's own output captured from an emulator.

Built with **GBDK-2020 4.5.0** (with SDCC 4.5.1). The GBDK runtime linked into the ROM is licensed GPL v2 with a linking exception, which does not extend to VACANT. Tested with **PyBoy 2.9** (LGPL v3, not distributed).

VACANT's source code and assets are released under the MIT licence (see `LICENSE`). This matches the earlier Synth Centric releases and was applied provisionally; the artist may choose different terms for the artwork and recordings.

*It has no purpose. No one needs anything from you. There is nowhere you are supposed to go.*
