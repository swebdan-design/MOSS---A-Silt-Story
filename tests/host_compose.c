/* Host tests for the portable composition modules (compose.c, garden.c,
 * memory.c). Builds with any C99 compiler; no Game Boy hardware involved.
 *   cc -std=c99 -Isource -Ibuild/gen tests/host_compose.c source/compose.c \
 *      source/garden.c source/memory.c -o build/host_compose
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compose.h"

#define MAXEV 200000
typedef struct { uint32_t t; uint8_t ch, note, env, prio, fx; } Rec;
static Rec rec[MAXEV];
static uint32_t nrec;
static uint32_t now32;

uint16_t compose_now(void) { return (uint16_t)now32; }
void compose_emit(const SndEv *e, uint8_t delay) {
    if (nrec < MAXEV) {
        Rec *r = &rec[nrec++];
        r->t = now32 + delay; r->ch = e->ch; r->note = e->note; r->env = e->env;
        r->prio = e->prio; r->fx = e->fx;
    }
}

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } \
                              else { printf("PASS: " __VA_ARGS__); printf("\n"); } } while (0)

static void run_frames(uint32_t frames) {
    uint32_t i;
    for (i = 0; i < frames; ++i) { ++now32; compose_frame(); if ((i & 3) == 0) garden_visual_tick(); }
}

static void reset_all(uint16_t seed) {
    nrec = 0; now32 = 0;
    mmem_reset();
    compose_init(seed);
}

/* Hash of the note sequence in a window of time (pitch classes + rhythm). */
static uint32_t window_hash(uint32_t from, uint32_t to, uint8_t ch) {
    uint32_t h = 2166136261u, i;
    for (i = 0; i < nrec; ++i) {
        if (rec[i].t < from || rec[i].t >= to || rec[i].ch != ch) continue;
        h ^= (uint32_t)(rec[i].note) | ((rec[i].t - from) / 6u) << 8;
        h *= 16777619u;
    }
    return h;
}

int main(void) {
    const uint32_t MIN = 3584; /* frames per minute at 59.73 Hz */
    uint32_t i, counts[4], lo = 255, hi = 0, silent_steps = 0;

    /* 1. Determinism */
    {
        uint32_t a, b;
        reset_all(1234); garden_preset(7); compose_scene(SCENE_POND, BEH_ALL);
        run_frames(MIN * 3); a = nrec; b = window_hash(0, MIN * 3, 0);
        reset_all(1234); garden_preset(7); compose_scene(SCENE_POND, BEH_ALL);
        run_frames(MIN * 3);
        CHECK(a == nrec && b == window_hash(0, MIN * 3, 0), "same seed -> identical %u events", (unsigned)a);
    }

    /* 2. Twenty-five minutes of autonomous pond, every behaviour on */
    reset_all(4321); garden_preset(0); pond.rain = RAIN_DRIZZLE; pond.echo = ECHO_MIST;
    compose_scene(SCENE_POND, BEH_ALL);
    run_frames(MIN * 25);
    memset(counts, 0, sizeof(counts));
    for (i = 0; i < nrec; ++i) {
        counts[rec[i].ch & 3]++;
        if (rec[i].ch < 3 && !(rec[i].fx & FX_DRONE)) {
            if (rec[i].note < lo) lo = rec[i].note;
            if (rec[i].note > hi) hi = rec[i].note;
        }
    }
    printf("25 min: %u events  pebble %u  living %u  water %u  env %u  notes %u..%u\n",
           (unsigned)nrec, (unsigned)counts[0], (unsigned)counts[1], (unsigned)counts[2],
           (unsigned)counts[3], (unsigned)lo, (unsigned)hi);
    CHECK(counts[0] > 500 && counts[1] > 50 && counts[2] > 30 && counts[3] > 50, "all four channels used");
    CHECK(lo >= 36 && hi <= 107, "pitches inside table range (%u..%u)", (unsigned)lo, (unsigned)hi);
    CHECK(nrec / 25 < 1500, "density bounded (%u events/min)", (unsigned)(nrec / 25));
    /* silence: steps (12 frames) with no pebble/living onset */
    {
        uint32_t s, steps = MIN * 25 / 12, j = 0;
        static uint8_t busy[3584 * 25 / 12 + 8];
        memset(busy, 0, sizeof(busy));
        for (j = 0; j < nrec; ++j) if (rec[j].ch < 2 && rec[j].t / 12 < steps) busy[rec[j].t / 12] = 1;
        for (s = 0; s < steps; ++s) silent_steps += !busy[s];
        printf("steps without a pulse onset: %u of %u (%.0f%%)\n", (unsigned)silent_steps,
               (unsigned)steps, 100.0 * silent_steps / steps);
        CHECK(silent_steps * 100 / steps >= 35, "the pond leaves silence (>=35%% of steps)");
    }
    /* non-repetition: compare every 30-second window of the melody with all others */
    {
        uint32_t w, v, nw = 50, dup = 0;
        uint32_t hs[50];
        for (w = 0; w < nw; ++w) hs[w] = window_hash(w * MIN / 2, (w + 1) * MIN / 2, 0);
        for (w = 0; w < nw; ++w) for (v = w + 1; v < nw; ++v) dup += hs[w] == hs[v];
        CHECK(dup == 0, "no two 30-second pebble windows identical over 25 min (%u dups)", (unsigned)dup);
    }

    /* 3. Stillness really is sparser */
    {
        uint32_t active, still;
        reset_all(99); garden_preset(0); pond.density = DENS_ACTIVE; compose_scene(SCENE_POND, BEH_ALL);
        run_frames(MIN * 5); active = nrec;
        reset_all(99); garden_preset(0); pond.density = DENS_STILL; compose_scene(SCENE_POND, BEH_ALL);
        run_frames(MIN * 5); still = nrec;
        printf("5 min events: active %u, still %u\n", (unsigned)active, (unsigned)still);
        CHECK(still * 10 < active * 7, "Still produces markedly fewer events");
    }

    /* 4. Presets differ audibly (channel balance, tempo, events) */
    {
        uint32_t p, sig[8][4];
        for (p = 0; p < 8; ++p) {
            reset_all(7); garden_preset((uint8_t)p); compose_scene(SCENE_POND, BEH_ALL);
            run_frames(MIN * 2);
            memset(sig[p], 0, sizeof(sig[p]));
            for (i = 0; i < nrec; ++i) sig[p][rec[i].ch & 3]++;
            printf("preset %u %-17s: pebble %4u living %4u water %4u env %4u\n", (unsigned)p,
                   garden_preset_name((uint8_t)p), (unsigned)sig[p][0], (unsigned)sig[p][1],
                   (unsigned)sig[p][2], (unsigned)sig[p][3]);
        }
        {
            uint32_t a, b, same = 0;
            for (a = 0; a < 8; ++a) for (b = a + 1; b < 8; ++b)
                same += !memcmp(sig[a], sig[b], sizeof(sig[a]));
            CHECK(same == 0, "eight presets produce distinct channel signatures");
        }
    }

    /* 5. Memory: repeated fifths make the pond prefer that interval, bounded */
    {
        uint32_t k, before, after;
        mmem_reset();
        before = mmem_interval_weight(3);
        for (k = 0; k < 2000; ++k) {
            comp.step += 4;
            mmem_player_note(LILY_BELL, (k & 1) ? 1 : 4); /* rows 4 <-> 1: three steps */
        }
        after = mmem_interval_weight(3);
        CHECK(before == 0 && after == 8, "interval memory learns repeated gesture (%u -> %u)",
              (unsigned)before, (unsigned)after);
        for (k = 0; k < 5; ++k) CHECK(mmem.interval[k] < 241, "counter %u bounded (%u)", (unsigned)k, mmem.interval[k]);
        mmem_reset();
        CHECK(mmem_interval_weight(3) == 0 && mmem.density == 128, "reset restores defaults");
    }

    /* 6. Garden geometry: stone waves reach SILT's original lilies */
    {
        reset_all(5); garden_preset(7); compose_scene(SCENE_POND, 0);
        run_frames(MIN);
        CHECK(garden.events > 40, "ripples cross lilies (%u crossings in 1 min)", (unsigned)garden.events);
        CHECK(garden_is_stone(1, 2) && garden_is_stone(6, 2) && !garden_is_stone(2, 2), "stones at SILT cells");
    }

    /* 7. Discoveries change the music: behaviours gate layers */
    {
        uint32_t none_l = 0, all_l = 0;
        reset_all(11); garden_story_default(); compose_scene(SCENE_POND, 0); run_frames(MIN * 3);
        for (i = 0; i < nrec; ++i) none_l += rec[i].ch == CH_LIVING || rec[i].ch == CH_WATER;
        reset_all(11); garden_story_default(); compose_scene(SCENE_POND, BEH_ALL); run_frames(MIN * 3);
        for (i = 0; i < nrec; ++i) all_l += rec[i].ch == CH_LIVING || rec[i].ch == CH_WATER;
        printf("living+water events: no discoveries %u, all %u\n", (unsigned)none_l, (unsigned)all_l);
        CHECK(none_l == 0 && all_l > 20, "first-day pond has no insects or water; discoveries add them");
    }

    printf("\n%s (%d failures)\n", failures ? "HOST TESTS FAILED" : "ALL HOST TESTS PASSED", failures);
    return failures ? 1 : 0;
}
