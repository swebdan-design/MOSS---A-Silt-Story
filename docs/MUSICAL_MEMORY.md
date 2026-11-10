# Musical memory

The pond gradually becomes familiar with how it is played. It never records
audio and never stores more than 40 bytes. Source: `source/memory.c`.

## What is remembered

| Field | Size | Updated when | Used for |
| --- | --- | --- | --- |
| `interval[5]` | 5 B | the player plays two notes within 64 steps | weighting melodic moves of 0–4 scale steps |
| `pitch[5]` | 5 B | every player note | (reported; available for weighting) |
| `voice[4]` | 4 B | player plays or places Bell / Reed / Root / Wood | how loudly crossings of that lily sound; insect activity |
| `rhythm[4]` | 4 B | gap between player notes: ≤2, ≤4, ≤8, more steps | weighting rhythm cells 2, 3, 4, 6 |
| `behaviour[8]` | 8 B | each minute at the pond, per active behaviour | (favourites; reported) |
| `scale[4]` | 4 B | each minute, and when the player chooses a scale | the ending's scale |
| `density` | 1 B | each minute: moves 1/8 of the way toward the chosen density | the ending's density |
| `motifs` | 1 B | a world motif is heard | (phrase memory is seeded directly) |
| `listen_min` | 2 B | each minute at the pond | (reported) |
| `last_row`, `last_step`, `gestures` | 4 B | player notes | interval and repeated-gesture tracking |

## Bounds

- Counters are bytes. When any counter in a group would pass 240, **the whole
  group is halved**. Proportions survive; nothing grows without limit; old
  habits slowly fade.
- Every weight read from memory is normalised to the group's largest value
  and capped (intervals 0–8, rhythm 0–6, voice 0–32), then **added** to the
  composer's own fixed weights. Memory can tilt a choice; it cannot remove one
  or make the pond unusable.
- Before MEMORY is found, interval memory acts at a quarter of its strength.

## Example

Playing the bell's fifth again and again (lowest row, then the second row)
fills `interval[3]`. Within a few dozen repetitions the melody's three-step
moves get up to 8 extra weight against the defaults (2–8), so the pond leans
toward that interval — usually still choosing others. The host test checks
exactly this and checks that all counters stay bounded.

## Persistence and reset

Memory is part of the save (`SaveData.mmem`), written whenever the game saves
(menu Save, each discovery, each new day, leaving the post-game pond). It is
validated with the rest of the save.

Settings → **Forget the pond** clears it after confirmation ("The pond's
memory is clear again. It will learn you slowly, all over."). Starting a new
summer also starts a fresh memory.
