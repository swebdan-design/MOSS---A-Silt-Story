# SILT Continuity Audit

*Prepared before any MOSS code was written. MOSS is a separate project; none of
the SILT sources were modified, rebuilt or redistributed.*

## Sources inspected

| Material | Supplied as | Status |
| --- | --- | --- |
| SILT — A Little Music Pond (Nintendo DS) | `silt-source.zip`: full source (`source/*.c`), tools, tests, docs, `release/` with `.nds`, artwork PNG/SVG, manual MD/PDF, liner notes, SHA256SUMS | **Completed release** — ROM, source and release package present |
| SILT manual PDF | `SILT_Manual.pdf` (separate upload, 15 pages) | Inspected; text matches `release/manual/SILT_Manual.md`. The byte content differs slightly from the PDF inside the source archive (different build of the same booklet) |
| SILT 2 — The Living Pond (Nintendo 3DS) | `SILT2_Source.zip`: full source, `classic/` port of SILT I core, PCM assets, `.3dsx`/`.smdh`, preview WAV, artwork, manual MD/PDF, liner notes, docs, screenshots | **Completed release** — binary, source and package present |
| SILT III — The Last Visit (Nintendo 3DS) | `SILT_III_Full_Source.zip`: full source, docs, manual MD, liner notes, tests, screenshots | **Source present, release binaries/artwork absent from archive**. No `.3dsx`, no album cover PNG/SVG. The README links `docs/EVOLUTION.md`, which is not in the archive |
| SILT III manual PDF | `SILT_III_Manual.pdf` (separate upload, 22 pages) | Inspected (title page and structure) |

No SILT ROM was executed for this audit; DS/3DS emulators were not needed to
establish continuity and are not part of the MOSS toolchain.

Evidence labels used below:

- **VERIFIED FROM SOURCE** — read directly in code or tool scripts.
- **VERIFIED FROM DOCUMENTATION** — stated in shipped README/manual/notes.
- **CONCEPTUAL ONLY** — an idea or tone, not a technical fact.
- **NOT AVAILABLE FOR INSPECTION** — referenced but missing.

## 1. Recurring colour and visual motifs

- **VERIFIED FROM SOURCE** (SILT `source/graphics.c`, `tools/create_artwork.py`):
  the identity is built from eight named RGB555 colours: NAVY (2,3,6), INK
  (2,5,8), WATER (2,7,9), GRID (3,9,11), MUTED (10,15,17), WHITE (27,29,26),
  GOLD (28,26,17), SAGE (14,22,17). Lily colours: Bell (11,20,14) green, Pluck
  (9,18,17) teal, Bass (8,13,19) blue, Click (19,17,11) ochre.
- **VERIFIED FROM DOCUMENTATION/ARTWORK**: night sky with sparse stars, a pixel
  crescent moon in gold, a horizon waterline, a rectangular pond with notched
  corners, faint concentric circles. SILT 2 adds reeds at both edges and a
  frog on the bank; the cover is otherwise deliberately the same composition.
- MOSS consequence: the Game Boy ROM uses only the DMG four shades, so colour
  continuity is carried by *shape* (moon, stones A/B, circles, notched pond
  frame, reeds). The high-resolution album art re-uses the SILT navy/cream/
  sage/gold relationships, shifted toward late-summer greens.

## 2. The pond

- **VERIFIED FROM SOURCE** (`pond.h`, `pond.c`): 8 columns × 5 rows. Stones at
  column 1 and column 6 of the middle row (row 2). Twelve starting lilies:
  Bells at (0,0) (3,0) (6,0) (7,1); Plucks at (2,1) (4,2) (5,3) (0,3); Basses at
  (1,4) (4,4); Clicks at (3,3) (7,4). Selecting the same tool on a lily removes it;
  a different tool replaces it.
- **VERIFIED FROM DOCUMENTATION**: "The pond is intimate" — a small rectangle,
  not a lake. SILT III: "The pond was here before you arrived. It will be here
  when you are gone."
- MOSS consequence: the pond instrument keeps the 8 × 5 grid and the two stones
  in exactly those cells, at 16 × 16 pixels per cell (128 × 80 pixels).

## 3. Instruments and timbral identities

- **VERIFIED FROM SOURCE** (`tools/generate_audio.py`): four generated PCM
  instruments. Bell: sine + decaying 2nd/3rd partials, 1.2 s, +12 semitones.
  Pluck: rounded triangle, 0.66 s. Bass: sine + 2nd/3rd partials, 0.95 s, −24
  semitones. Click: 85 ms. 22,050 Hz PCM16 on DS hardware channels 0–7.
- SILT 2 (**VERIFIED FROM DOCUMENTATION**): keeps the originals and adds
  real-time synthesised Reed and Glass lilies, substrate objects (Stone, Shell,
  Root, Sediment) and three organisms (frog, insect, fish).
- SILT III (**VERIFIED FROM SOURCE/DOCUMENTATION**): a six-voice drone "Bed"
  and seven families "Reeds, Insects, Glass, Stone, Root, Water, Echo".
- MOSS consequence: MOSS cannot play PCM. Its four lily types map the SILT
  quartet onto the four DMG channels: **Bell** (pulse 1), **Reed** (pulse 2,
  swelling envelope), **Root** (wave channel — the bass), **Wood** (noise,
  7-bit tonal mode — the click). The DS sound engine is *not* ported.

## 4. Ripple and stone mechanics

- **VERIFIED FROM SOURCE** (`music.c`, `ripple.c`, `ripple.h`): sixteenth-note
  clock. Calm: Stone A emits every 48 steps, Stone B every 64 steps offset 24.
  Drift: 32 and 44 steps, B offset 13. Radius grows 12 px per step to 276 px.
  At most 8 waves; extra waves are dropped. A lily sounds once when the wave
  front's squared radius passes its cell centre (explicit 40-bit visited mask).
- **VERIFIED FROM SOURCE** (`rain.c`): xorshift32 rain, Drizzle 40–64 steps,
  Rain 16–28, Shower 4–8 settling to Rain after 128 steps; rain waves are
  smaller and always leave two wave slots free.
- **VERIFIED FROM DOCUMENTATION** (`AUDIO_EFFECTS.md`): Mist = one quiet repeat
  a sixteenth later; Deep = two repeats at two and three sixteenths, different
  stereo positions; repeats never feed back; Click stays dry.
- MOSS consequence: the cycles 48/64 (Calm) and 32/44 (Drift), the visited-mask
  crossing rule, the rain intervals and the "never feed back" echo rule are
  reused as *rules*, re-implemented in portable C for the Game Boy.

## 5. Scales

- **VERIFIED FROM SOURCE**: D minor pentatonic (rows high→low C5 A4 G4 F4 D4 =
  MIDI 72 69 67 65 62) and C major pentatonic (69 67 64 62 60). Default 76 BPM,
  range 50–140.
- SILT III (**VERIFIED FROM SOURCE**) extends this to eight degree sets.
- MOSS consequence: same default (D minor pentatonic, 76 BPM), same row order
  (higher rows are higher notes), plus Open Fifths and Suspended environments.

## 6. Atmosphere and pacing

- **VERIFIED FROM DOCUMENTATION**: "There is nothing to win or finish." Silence
  is part of the arrangement. Additions start Off/Dry. The pond keeps playing
  while Info is open. Long interruptions never cause bursts of queued notes.
- **CONCEPTUAL ONLY**: dusk, breathing, counting, patience; the listener
  establishes conditions; the system performs within them.

## 7. Typography and title identity

- **VERIFIED FROM SOURCE** (`font.h`): an original hand-set 5 × 7 pixel
  alphabet — upper case, digits and a "small lower-case companion for the
  Synth Centric signature". Covers set the title large in this alphabet,
  subtitle in MUTED, "by Synth Centric" in SAGE, all centred.
- MOSS consequence: MOSS's in-game variable-width font is derived from these
  5 × 7 glyphs (project-owned, MIT), with new punctuation. The title screen and
  covers repeat the title / subtitle / "by Synth Centric" stack and the moon.

## 8. Narrative language

- **VERIFIED FROM DOCUMENTATION** (`The_Pond.md`, `The_Return.md`): second
  person, present tense, very short paragraphs, single-word lines ("One.").
  No exclamation, no lore. SILT 2: "You have been here before. The two stones
  remain." SILT III ends "The pond remains. Thank you for listening." and
  "THE STORY ENDS. THE INSTRUMENT REMAINS."
- MOSS consequence: MOSS is first person and past tense (a boy's notebook),
  which places it *before* the second-person SILT texts without stating it.
  Sentences stay short. The ending line closes a circle with SILT's opening.

## 9. Album-art conventions

- **VERIFIED FROM ARTWORK** (SILT, SILT 2 covers): square, 3000 and 1000 px PNG
  plus editable SVG built from code; flat pixel shapes, `shape-rendering:
  crispEdges`; navy ground; title upper third; pond in the lower half.
- **NOT AVAILABLE FOR INSPECTION**: SILT III cover files (not in archive).
- MOSS consequence: same production method (SVG and PNG generated from one
  Python source), a related but new composition: a boy, a willow and a small
  pond at late-summer dusk, rendered as enlarged four-shade Game Boy pixels.

## 10. Credits and packaging

- **VERIFIED FROM DOCUMENTATION**: "Dan Schwebel, known as Synth Centric, is an
  Australian beat maker, music producer, writer and educator." Releases ship
  README, manual MD+PDF, liner notes (an artistic statement and a short prose
  piece), artwork, SHA256SUMS, TESTING with PASS / NOT TESTED distinctions,
  and MIT-licensed source ("Copyright (c) 2026 SILT contributors").
- MOSS consequence: identical package shape; the licence decision for MOSS is
  left documented for the creator (see `LICENSES.md`).

## Canonical relationship chosen for MOSS

**CONCEPTUAL ONLY.** MOSS never names SILT inside the fiction. The pond has the
same two stones. The boy leaves a third, small stone on the bank; SILT's later
covers show only two in the water. The station bell's interval (a fifth, D–A)
is the interval between the two stones' rows in the pond, and the SILT
releases' most common pentatonic leap. SILT III's final preset is called
"The Pond Remains"; MOSS's eighth preset reuses that name deliberately as a
bridge, and its last line answers SILT's first: *"I thought I had found the
pond. But it was always here."*

## Limitations of this audit

- SILT III release binaries and cover artwork were not supplied.
- No SILT ROMs were run; behaviour claims rely on source and documentation.
- No SILT melody is quoted: SILT has no authored melodies (its music is
  emergent). MOSS's motifs are new and are documented in `AUDIO_ENGINE.md`.
