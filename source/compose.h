/* MOSS — generative composition (portable C: Game Boy bank 1, or host tests).
 *
 * The composer never touches hardware. It decides *what* and *when*, and
 * hands timestamped events to compose_emit(), which the platform implements
 * (snd_play() on the Game Boy; a recorder in host tests).
 */
#ifndef MOSS_COMPOSE_H
#define MOSS_COMPOSE_H
#include <stdint.h>

#ifdef __SDCC
#include <gb/gb.h>
#define MB BANKED
#else
#define MB
#endif

#include "audio.h"

/* The eight ways of listening (notebook discoveries) */
#define BEH_WIND    0x01
#define BEH_WATER   0x02
#define BEH_INSECTS 0x04
#define BEH_WOOD    0x08
#define BEH_RAIN    0x10
#define BEH_ECHO    0x20
#define BEH_STILL   0x40
#define BEH_MEMORY  0x80
#define BEH_ALL     0xFF

/* Scenes: each has a lightweight musical profile */
enum {
    SCENE_SILENT, SCENE_TITLE, SCENE_COTTAGE, SCENE_VILLAGE, SCENE_WOOD,
    SCENE_CREEK, SCENE_RAILWAY, SCENE_HILL, SCENE_CLEARING, SCENE_POND_SHORE,
    SCENE_POND, SCENE_ENDING, SCENE_NIGHT, SCENE_COUNT
};

/* Lily types (SILT's Bell / Pluck / Bass / Click, re-voiced for the DMG) */
enum { LILY_NONE, LILY_BELL, LILY_REED, LILY_ROOT, LILY_WOOD, LILY_COUNT };

enum { SCALE_DMINOR, SCALE_CMAJOR, SCALE_FIFTHS, SCALE_SUS, SCALE_COUNT };
enum { DENS_STILL, DENS_SPARSE, DENS_GENTLE, DENS_ACTIVE, DENS_COUNT };
enum { RAIN_OFF, RAIN_DRIZZLE, RAIN_RAIN, RAIN_SHOWER, RAIN_COUNT };
enum { ECHO_DRY, ECHO_MIST, ECHO_DEEP, ECHO_COUNT };

/* Authored motifs that recur through the world */
enum { MOTIF_POND, MOTIF_BELL, MOTIF_MAE, MOTIF_IRIS, MOTIF_TRAIN,
       MOTIF_CHIME, MOTIF_ANSWER, MOTIF_COUNT };

#define POND_COLS 8
#define POND_ROWS 5
#define POND_CELLS 40
#define RIPPLE_LIMIT 6
#define CELL_PX 16

/* --- persistent: the pond's configuration (saved) --- */
typedef struct {
    uint8_t pads[POND_CELLS];
    uint8_t scale, density, rain, echo, drift, bpm, water, insects, hold, wave;
} PondConfig;

/* --- persistent: the pond's musical memory (saved, bounded) --- */
typedef struct {
    uint8_t interval[5];  /* |pentatonic steps| between consecutive player notes */
    uint8_t pitch[5];     /* rows the player sounds, high->low */
    uint8_t voice[4];     /* lily types the player places or plays */
    uint8_t rhythm[4];    /* gaps between player gestures: 1-2, 3-4, 5-8, 9+ steps */
    uint8_t behaviour[8]; /* time spent with each behaviour active */
    uint8_t scale[4];
    uint8_t density;      /* EMA 0 (sparse) .. 255 (active) */
    uint8_t motifs;       /* bit per motif heard in the world */
    uint16_t listen_min;  /* minutes spent at the pond */
    uint8_t last_row, last_gap_step_lo;
    uint16_t last_step;
    uint8_t gestures, reserved;
} MusicMemory;

typedef struct {
    int16_t x, y;      /* centre, pond pixels */
    int16_t r, prev;   /* radius this step / previous step */
    uint8_t mask[5];   /* 40-bit visited mask */
    uint8_t source;    /* 0 = stone A, 1 = stone B, 2 = player, 3 = rain */
    uint8_t active;
    uint8_t life;      /* max radius / 2 */
} Ripple;

typedef struct {
    Ripple rip[RIPPLE_LIMIT];
    uint8_t flash[POND_CELLS];
    uint16_t rain_next;
    uint16_t events, dropped;
} Garden;

typedef struct {
    uint8_t deg[6], gap[6], len;
} Phrase;

typedef struct {
    /* settings */
    uint8_t scene, behaviours, bpm, scale, density, rain, echo, drift, hold, wave;
    uint8_t water, insects, quiet;     /* quiet: duck music (listening state) */
    /* time */
    uint16_t step;
    uint16_t next_at;  /* audio tick of next step */
    uint8_t next_frac;
    uint16_t step_fp;  /* frames per step, 8.8 */
    /* random streams (separate so weather never disturbs melody) */
    uint16_t rng, rng_env;
    /* harmony */
    uint8_t chord, chord_left, drone_note;
    /* melody */
    uint8_t mel_deg, phrase_left, rest_left, note_wait, hush;
    uint8_t recalling, recall_pos;
    Phrase cur, recall;
    Phrase bank[4];
    uint8_t bank_next;
    /* insects */
    uint8_t reply_deg, reply_in;
    /* echo ping-pong */
    uint8_t echo_flip;
    /* tide: slow activity curves */
    uint16_t tide_a, tide_b;
    /* stats */
    uint16_t notes, rests;
} Composer;

extern Composer comp;
extern Garden garden;
extern PondConfig pond;
extern MusicMemory mmem;

/* platform hook */
void compose_emit(const SndEv *e, uint8_t delay);
uint16_t compose_now(void);

/* compose.c */
void compose_init(uint16_t seed) MB;
void compose_scene(uint8_t scene, uint8_t behaviours) MB;
void compose_frame(void) MB;
void compose_set_tempo(uint8_t bpm) MB;
void compose_apply_pond(void) MB;
void compose_motif(uint8_t motif, uint8_t delay) MB;
void compose_player_note(uint8_t type, uint8_t row, uint8_t col, uint8_t player) MB;
void compose_hush(uint8_t steps) MB;
uint8_t compose_scale_midi(uint8_t degree) MB;
uint8_t compose_tide(void) MB;

/* garden.c */
void garden_reset(void) MB;
void garden_step(void) MB;
uint8_t garden_spawn(int16_t x, int16_t y, uint8_t source) MB;
uint8_t garden_is_stone(uint8_t col, uint8_t row) MB;
void garden_preset(uint8_t index) MB;
void garden_story_default(void) MB;
void garden_visual_tick(void) MB;
uint8_t garden_count(void) MB;
const char *garden_preset_name(uint8_t index) MB;
void garden_preset_name_copy(uint8_t index, char *out) MB;

/* memory.c */
void mmem_reset(void) MB;
void mmem_player_note(uint8_t type, uint8_t row) MB;
void mmem_minute(void) MB;
void mmem_choose(uint8_t what, uint8_t value) MB;
uint8_t mmem_interval_weight(uint8_t absstep) MB;
uint8_t mmem_pitch_weight(uint8_t row) MB;
uint8_t mmem_voice_bias(uint8_t type) MB;
uint8_t mmem_favourite_scale(void) MB;
uint8_t mmem_preferred_density(void) MB;
uint8_t mmem_rhythm_gap(uint8_t r) MB;

#define MM_SCALE 0
#define MM_DENSITY 1
#endif
