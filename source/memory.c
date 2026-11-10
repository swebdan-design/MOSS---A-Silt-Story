/* MOSS — the pond's musical memory.
 *
 * Small saturating counters, saved in SRAM. When one counter in a group
 * reaches its ceiling the whole group is halved, so proportions survive and
 * nothing grows without bound. Every weight read from here is capped, and
 * added to (never multiplied into) the composer's own defaults, so memory
 * can colour the pond but never take it over or make it unusable.
 */
#ifdef __SDCC
#pragma bank 1
#endif
#include <string.h>
#include "compose.h"

MusicMemory mmem;

#define CEIL 240

static void bump(uint8_t *a, uint8_t n, uint8_t i, uint8_t amount) {
    uint8_t k;
    if (i >= n) return;
    if ((uint16_t)a[i] + amount >= CEIL) for (k = 0; k < n; ++k) a[k] >>= 1;
    a[i] += amount;
}

void mmem_reset(void) MB {
    memset(&mmem, 0, sizeof(mmem));
    mmem.density = 128;
    mmem.last_row = 0xFF;
}

void mmem_player_note(uint8_t type, uint8_t row) MB {
    uint16_t gap;
    bump(mmem.pitch, 5, row, 2);
    if (type >= LILY_BELL && type <= LILY_WOOD) bump(mmem.voice, 4, (uint8_t)(type - 1), 2);
    if (mmem.last_row < POND_ROWS) {
        uint8_t d = mmem.last_row > row ? mmem.last_row - row : row - mmem.last_row;
        gap = comp.step - mmem.last_step;
        if (gap < 64) {
            bump(mmem.interval, 5, d, 3);
            bump(mmem.rhythm, 4, gap <= 2 ? 0 : gap <= 4 ? 1 : gap <= 8 ? 2 : 3, 2);
            if (d == 0 && mmem.gestures < 250) ++mmem.gestures;
        }
    }
    mmem.last_row = row;
    mmem.last_step = comp.step;
}

/* Called once per minute of listening at the pond. */
void mmem_minute(void) MB {
    uint8_t i;
    if (mmem.listen_min < 60000u) ++mmem.listen_min;
    for (i = 0; i < 8; ++i)
        if (comp.behaviours & (uint8_t)(1u << i)) bump(mmem.behaviour, 8, i, 1);
    /* density preference drifts slowly toward what is chosen */
    {
        static const uint8_t target[DENS_COUNT] = {0, 70, 160, 255};
        uint8_t t = target[comp.density % DENS_COUNT];
        mmem.density = (uint8_t)(((uint16_t)mmem.density * 7 + t) >> 3);
    }
    bump(mmem.scale, 4, comp.scale & 3, 1);
}

void mmem_choose(uint8_t what, uint8_t value) MB {
    if (what == MM_SCALE) bump(mmem.scale, 4, value & 3, 4);
}

/* 0..8 */
uint8_t mmem_interval_weight(uint8_t absstep) MB {
    uint8_t i, top = 1;
    if (absstep > 4) return 0;
    for (i = 0; i < 5; ++i) if (mmem.interval[i] > top) top = mmem.interval[i];
    return (uint8_t)(((uint16_t)mmem.interval[absstep] * 8) / top);
}

uint8_t mmem_pitch_weight(uint8_t row) MB {
    uint8_t i, top = 1;
    if (row > 4) return 0;
    for (i = 0; i < 5; ++i) if (mmem.pitch[i] > top) top = mmem.pitch[i];
    return (uint8_t)(((uint16_t)mmem.pitch[row] * 8) / top);
}

/* 0..32: how much the player favours a voice */
uint8_t mmem_voice_bias(uint8_t type) MB {
    uint8_t i, top = 1;
    if (type < LILY_BELL || type > LILY_WOOD) return 0;
    for (i = 0; i < 4; ++i) if (mmem.voice[i] > top) top = mmem.voice[i];
    return (uint8_t)(((uint16_t)mmem.voice[type - 1] * 32) / top);
}

uint8_t mmem_rhythm_gap(uint8_t r) MB {
    uint8_t i, top = 1;
    if (r > 3) return 0;
    for (i = 0; i < 4; ++i) if (mmem.rhythm[i] > top) top = mmem.rhythm[i];
    return (uint8_t)(((uint16_t)mmem.rhythm[r] * 6) / top);
}

uint8_t mmem_favourite_scale(void) MB {
    uint8_t i, best = 0;
    for (i = 1; i < 4; ++i) if (mmem.scale[i] > mmem.scale[best]) best = i;
    return best;
}

uint8_t mmem_preferred_density(void) MB {
    return mmem.density < 40 ? DENS_STILL : mmem.density < 110 ? DENS_SPARSE :
           mmem.density < 200 ? DENS_GENTLE : DENS_ACTIVE;
}
