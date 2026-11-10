/* MOSS — the pond garden: SILT's ripple sequencer, re-built for the DMG.
 *
 * 8 x 5 cells of 16 px. Stones A and B sit in column 1 and column 6 of the
 * middle row, as in SILT. Calm: A every 48 steps, B every 64 (offset 24).
 * Drift (WIND): 32 / 44 (offset 13). A lily sounds once when a wave front
 * passes its centre (explicit visited mask, integer squared distances).
 */
#ifdef __SDCC
#pragma bank 1
#endif
#include <string.h>
#include "compose.h"

Garden garden;
PondConfig pond;

#define SPEED 6
#define STONE_LIFE 73 /* radius / 2: SILT's 276 px across 30 px cells, scaled */

static const int16_t stone_x[2] = {1 * CELL_PX + 8, 6 * CELL_PX + 8};
static const int16_t stone_y = 2 * CELL_PX + 8;

static uint16_t sq(int16_t v) {
    if (v < 0) v = -v;
    return (uint16_t)v * (uint16_t)v;
}

uint8_t garden_is_stone(uint8_t col, uint8_t row) MB {
    return row == 2 && (col == 1 || col == 6);
}

uint8_t garden_spawn(int16_t x, int16_t y, uint8_t source) MB {
    uint8_t i;
    for (i = 0; i < RIPPLE_LIMIT; ++i) {
        Ripple *r = &garden.rip[i];
        if (!r->active) {
            memset(r, 0, sizeof(*r));
            r->x = x; r->y = y; r->prev = -1; r->source = source; r->active = 1;
            r->life = source == 3 ? (uint8_t)(13 + 3 * comp.rain) : source == 2 ? 50 : STONE_LIFE;
            return 1;
        }
    }
    ++garden.dropped;
    return 0;
}

static uint8_t rain_active(void) {
    uint8_t i, n = 0;
    for (i = 0; i < RIPPLE_LIMIT; ++i) n += garden.rip[i].active && garden.rip[i].source == 3;
    return n;
}

static uint8_t active_count(void) {
    uint8_t i, n = 0;
    for (i = 0; i < RIPPLE_LIMIT; ++i) n += garden.rip[i].active;
    return n;
}

static uint16_t rain_rng = 0x7261;
static uint16_t rrand(void) {
    uint16_t x = rain_rng;
    x ^= x << 7; x ^= x >> 9; x ^= x << 8;
    return rain_rng = x;
}

static void rain_step(void) {
    uint8_t col, row;
    if (!comp.rain || !(comp.behaviours & BEH_RAIN)) return;
    if (comp.step < garden.rain_next) return;
    {
        uint16_t g = rrand();
        uint8_t interval = comp.rain == RAIN_DRIZZLE ? 40 + 4 * (g % 7) :
                           comp.rain == RAIN_RAIN ? 16 + 4 * (g % 4) : 4 + 2 * (g % 3);
        garden.rain_next = comp.step + interval;
    }
    /* Leave two wave slots for the stones and the player. */
    if (rain_active() >= comp.rain || active_count() >= RIPPLE_LIMIT - 2) { ++garden.dropped; return; }
    col = (uint8_t)(rrand() % POND_COLS);
    row = (uint8_t)(rrand() % POND_ROWS);
    if (garden_is_stone(col, row)) row = (uint8_t)((row + 1) % POND_ROWS);
    garden_spawn((int16_t)(col * CELL_PX + 8 + (int8_t)(rrand() % 7) - 3),
                 (int16_t)(row * CELL_PX + 8 + (int8_t)(rrand() % 5) - 2), 3);
}

void garden_step(void) MB {
    uint8_t i, c;
    uint16_t s = comp.step;
    uint8_t due_a, due_b;
    if (comp.drift) { due_a = (s % 32) == 0; due_b = (s % 44) == 13; }
    else { due_a = (s % 48) == 0; due_b = (s % 64) == 24; }
    if (due_a) garden_spawn(stone_x[0], stone_y, 0);
    if (due_b) garden_spawn(stone_x[1], stone_y, 1);
    rain_step();
    for (i = 0; i < RIPPLE_LIMIT; ++i) {
        Ripple *r = &garden.rip[i];
        uint16_t outer, inner;
        if (!r->active) continue;
        r->prev = r->r;
        r->r += SPEED;
        outer = sq(r->r);
        inner = r->prev > 0 ? sq(r->prev) : 0;
        for (c = 0; c < POND_CELLS; ++c) {
            uint8_t type = pond.pads[c];
            uint8_t col, row;
            uint16_t d2;
            if (!type) continue;
            if (r->mask[c >> 3] & (uint8_t)(1u << (c & 7))) continue;
            col = c & 7; row = c >> 3;
            d2 = sq((int16_t)(col * CELL_PX + 8) - r->x) + sq((int16_t)(row * CELL_PX + 8) - r->y);
            if (d2 <= outer && (r->prev <= 0 || d2 > inner || r->prev == r->r)) {
                r->mask[c >> 3] |= (uint8_t)(1u << (c & 7));
                ++garden.events;
                garden.flash[c] = 14;
                compose_player_note(type, row, col, 0);
            }
        }
        if (r->r >= (int16_t)r->life * 2) r->active = 0;
    }
}

void garden_visual_tick(void) MB {
    uint8_t c;
    for (c = 0; c < POND_CELLS; ++c) if (garden.flash[c]) --garden.flash[c];
}

void garden_reset(void) MB {
    memset(&garden, 0, sizeof(garden));
}

uint8_t garden_count(void) MB {
    uint8_t c, n = 0;
    for (c = 0; c < POND_CELLS; ++c) n += pond.pads[c] != 0;
    return n;
}

/* ---- presets: lilies (col,row,type) and conditions ---- */
#define L(c, r, t) (uint8_t)(((c) << 5) | ((r) << 2) | ((t) - 1))
typedef struct {
    const char *name;
    uint8_t lilies[14];
    uint8_t n;
    uint8_t scale, density, rain, echo, drift, bpm, water, insects, wave;
} Preset;

static const Preset presets[8] = {
    {"First Morning",
     {L(0,0,1), L(3,0,1), L(6,1,1), L(2,1,2), L(4,3,2), L(1,4,3), L(5,4,3)}, 7,
     SCALE_DMINOR, DENS_GENTLE, RAIN_OFF, ECHO_DRY, 0, 76, 1, 0, WAVE_SOFT},
    {"The Creek",
     {L(0,1,1), L(2,0,1), L(5,1,1), L(7,0,1), L(3,3,3), L(0,4,3), L(5,4,3), L(7,3,2), L(4,2,2)}, 9,
     SCALE_CMAJOR, DENS_GENTLE, RAIN_OFF, ECHO_MIST, 0, 84, 1, 1, WAVE_WARM},
    {"In the Reeds",
     {L(0,0,2), L(2,1,2), L(3,3,2), L(5,0,2), L(7,2,2), L(4,4,2), L(7,4,3), L(0,3,1)}, 8,
     SCALE_DMINOR, DENS_SPARSE, RAIN_OFF, ECHO_DRY, 1, 70, 1, 1, WAVE_REED},
    {"Hollow Wood",
     {L(0,0,4), L(2,2,4), L(4,1,4), L(5,3,4), L(7,1,4), L(3,4,3), L(1,0,1), L(6,4,1)}, 8,
     SCALE_SUS, DENS_ACTIVE, RAIN_OFF, ECHO_DRY, 0, 92, 1, 0, WAVE_HOLLOW},
    {"Summer Rain",
     {L(0,0,1), L(4,0,1), L(7,1,1), L(2,3,2), L(5,2,2), L(3,4,3), L(6,4,4)}, 7,
     SCALE_DMINOR, DENS_GENTLE, RAIN_RAIN, ECHO_MIST, 0, 76, 1, 1, WAVE_SOFT},
    {"Quiet Clearing",
     {L(3,0,1), L(4,4,3), L(0,2,2)}, 3,
     SCALE_FIFTHS, DENS_STILL, RAIN_OFF, ECHO_DEEP, 1, 60, 1, 0, WAVE_SOFT},
    {"Before Leaving",
     {L(0,1,1), L(3,0,1), L(7,0,1), L(2,2,2), L(5,3,2), L(3,4,3), L(7,4,3), L(0,4,4)}, 8,
     SCALE_CMAJOR, DENS_SPARSE, RAIN_DRIZZLE, ECHO_MIST, 1, 68, 1, 1, WAVE_SOFT},
    /* SILT's original twelve-lily garden, cell for cell:
       Bell->Bell, Pluck->Reed, Bass->Root, Click->Wood. */
    {"The Pond Remains",
     {L(0,0,1), L(3,0,1), L(6,0,1), L(7,1,1), L(2,1,2), L(4,2,2), L(5,3,2), L(0,3,2),
      L(1,4,3), L(4,4,3), L(3,3,4), L(7,4,4)}, 12,
     SCALE_DMINOR, DENS_GENTLE, RAIN_OFF, ECHO_DRY, 0, 76, 1, 1, WAVE_SOFT},
};

const char *garden_preset_name(uint8_t index) MB {
    return presets[index & 7].name;
}

void garden_preset(uint8_t index) MB {
    const Preset *p = &presets[index & 7];
    uint8_t i;
    memset(pond.pads, 0, sizeof(pond.pads));
    for (i = 0; i < p->n; ++i) {
        uint8_t v = p->lilies[i];
        pond.pads[((v >> 2) & 7) * POND_COLS + (v >> 5)] = (uint8_t)((v & 3) + 1);
    }
    pond.scale = p->scale; pond.density = p->density; pond.rain = p->rain;
    pond.echo = p->echo; pond.drift = p->drift; pond.bpm = p->bpm;
    pond.water = p->water; pond.insects = p->insects; pond.wave = p->wave;
    pond.hold = 0;
    garden_reset();
}

/* The pond when the boy first finds it: two stones and three quiet lilies. */
void garden_story_default(void) MB {
    memset(&pond, 0, sizeof(pond));
    pond.pads[0 * POND_COLS + 3] = LILY_BELL;
    pond.pads[1 * POND_COLS + 5] = LILY_BELL;
    pond.pads[3 * POND_COLS + 2] = LILY_BELL;
    pond.scale = SCALE_DMINOR;
    pond.density = DENS_SPARSE;
    pond.bpm = 76;
    pond.water = 1;
    pond.insects = 1;
    pond.wave = WAVE_SOFT;
    garden_reset();
}
