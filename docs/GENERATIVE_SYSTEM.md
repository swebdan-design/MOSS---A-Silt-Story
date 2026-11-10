# Generative system

The pond establishes conditions; the music happens inside them. This is the
idea MOSS inherits from SILT. Source: `source/compose.c` (composer),
`source/garden.c` (ripple garden). Both are portable C and are compiled for
the host by `make test`.

## Time

A sixteenth-note **step** clock runs from audio ticks: ticks per step =
60.24 × 60 / (4 × BPM), kept in 8.8 fixed point so tempo never drifts.
Default 76 BPM (SILT's), range 52–120 in the pond menu.

## Layers

Each step, the composer runs these in order:

1. **Harmony.** A slow Markov chain chooses the drone's scale degree every
   32–64 steps (longer in Still). Weights favour movement to the fifth and
   fourth and back home. With WATER found, channel 3 glides to the new note.
   **Hold** freezes harmony.
2. **Garden** (pond scenes only). SILT's ripple sequencer: stones A and B emit
   waves on their own cycles (Calm 48/64 steps, B offset 24; Drift 32/44,
   offset 13). A wave grows 6 px per step; a lily sounds once when the wave
   front passes its centre (40-bit visited mask, squared distances). At most
   six waves; rain waves always leave two slots for the stones and the
   player. Row sets pitch (higher rows higher), column sets pan.
3. **Pebbles (melody).** Phrases of 2–5 notes separated by rests. Each next
   note is a weighted choice among moves of 0, ±1, ±2, ±3 scale steps and
   +5 (an octave), multiplied by its consonance with the drone. The last note
   of a phrase leans toward a consonance. Gaps between notes come from
   rhythm cells {2, 3, 4, 6} steps.
4. **Phrase memory.** Finished phrases go into a four-slot bank. A new phrase
   may instead *recall* a stored one (20 % of the time, 70 % with MEMORY
   found): transposed by up to a step, possibly with one note altered and the
   rhythm stretched. Motifs heard in the world enter the same bank.
5. **Living sounds** (INSECTS). After a note the insects may answer with a
   short trill a few steps later, at an interval chosen from 0, +2, +3, +5,
   −2 steps. **Acoustic niche:** they do not answer notes in the top
   register, leaving it to the melody. Between phrases they make sparse calls
   of their own, more at night, fewer in Still.
6. **Environment.** Rain drops at random sub-step offsets (Drizzle, Rain,
   Shower densities), wind gusts (WIND), creek babble, wood knocks (HOLLOW
   WOOD), the cottage clock.
7. **Echo** (ECHO). Pebble and reed notes repeat three steps later on the
   other pulse channel, 5 volume steps quieter; Deep adds a second repeat at
   six steps. Repeats never repeat (no feedback). Wood stays dry, as SILT's
   Click did.

## Silence and long-form variation

- **Rests** between phrases scale with density (Still ×4, Sparse ×2) and with
  the *tide*.
- **The tide** is the sum of two triangle waves with periods of 1024 and 1536
  steps (about 3.4 and 5 minutes at 76 BPM). Their combination does not
  repeat for about 10 minutes and shapes how long the pond rests.
- **Hushes:** occasionally (≈1 in 128 steps between phrases) the melody and
  insects stop for 16–48 steps while the water continues. Stillness lengthens
  them.
- Two independent 16-bit xorshift streams drive melody and weather, so rain
  never changes the melody's sequence. Seeds are saved; the same seed gives
  the same performance (host-tested).

Host test results (`tests/host_compose.c`): over 25 simulated minutes no two
30-second windows of the melody are identical; about two thirds of steps have
no new pulse onset; Still produces markedly fewer events than Active; all
four channels are used; every pitch stays inside the table.

## Density

| Setting | Effect |
| --- | --- |
| Still (needs STILLNESS) | rests ×4, quieter notes, 40 % of lily crossings skipped, longer harmony |
| Sparse | rests ×2, phrases 2–4 notes |
| Gentle | default |
| Active | rests halved, more short gaps, phrases up to 5 notes |

## Scales

Five degrees per scale, as SILT used five rows:

| Scale | Degrees | Notes from D (or C) |
| --- | --- | --- |
| D minor pentatonic (default) | 0 3 5 7 10 | D F G A C |
| C major pentatonic | 0 2 4 7 9 | C D E G A |
| Open fifths | 0 7 12 14 19 | D A D E A |
| Suspended | 0 2 5 7 9 | D E G A B |

Bell lilies sound two octaves above the row's degree (MIDI 74–84 in D minor,
SILT's Bell register), reeds one octave (62–72, SILT's Pluck), roots below
(38–48, SILT's Bass).

## Discoveries as musical behaviours

| Found | What it adds to the pond and the world |
| --- | --- |
| (nothing) | pebbles and bell lilies only; the world is nearly silent |
| WIND | Reed lilies; the Wind (Drift) setting; gusts; slower drone glides |
| WATER | Root lilies; the drone and its harmony; Water on/off |
| INSECTS | insect replies and calls; the reed creature shows itself |
| HOLLOW WOOD | Wood lilies; knocks in the wood |
| RAIN | Rain setting (rain ripples on the pond) |
| ECHO | Echo setting (Mist, Deep) |
| STILLNESS | the Still density |
| MEMORY | phrase recall ×3.5; memory weights at full strength |

The overworld uses the same composer with a profile per place (rest length,
melody range, insect rate, drone level, ambience, tempo), and the same
behaviour gates — so the boy's world gains sounds exactly as he learns them.

## Presets (post-game)

Each preset sets lilies **and** conditions, so they behave differently, not
just with a different seed:

| # | Name | Garden | Conditions |
| --- | --- | --- | --- |
| 1 | First Morning | 7 lilies: bells, reeds, roots | D minor, Gentle, Calm, dry, 76 |
| 2 | The Creek | 9 lilies, roots low | C major, Gentle, Mist echo, warm wave, 84 |
| 3 | In the Reeds | mostly reeds | D minor, Sparse, Drift, reed wave, 70 |
| 4 | Hollow Wood | five wood lilies | Suspended, Active, hollow wave, 92 |
| 5 | Summer Rain | 7 lilies | D minor, Gentle, Rain, Mist, 76 |
| 6 | Quiet Clearing | 3 lilies | Open fifths, Still, Deep echo, Drift, 60 |
| 7 | Before Leaving | 8 lilies | C major, Sparse, Drizzle, Mist, Drift, 68 |
| 8 | The Pond Remains | SILT's original 12-lily garden, cell for cell | D minor, Gentle, Calm, dry, 76 |

## The final composition

At the ending the pond plays the player's own garden, with the scale they
used most, their preferred density (capped at Gentle), Drift if WIND was found,
at least Mist echo and Drizzle rain if those were found, and phrase memory
seeded with every motif heard on the way. If the garden is nearly empty the
"Before Leaving" garden is used. A fresh game's first pond (three bell lilies,
no water, no insects) cannot sound like this.
