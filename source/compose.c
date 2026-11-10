/* MOSS — generative composer.
 *
 * Layers, each tied to a way of listening:
 *   pebbles  (pulse 1)  phrase-based weighted melody with rests and recall
 *   living   (pulse 2)  insect replies and calls             [INSECTS]
 *   water    (wave)     drone with slow harmonic movement     [WATER]
 *   environment (noise) rain, wind rustle, wood, clock        [RAIN/WIND/WOOD]
 *   echo                ping-pong repeats across the pulses    [ECHO]
 *   stillness           fewer events, longer silences          [STILL]
 *   memory              phrase recall, motifs, learned weights [MEMORY]
 * In SCENE_POND / SCENE_ENDING the SILT ripple garden (garden.c) also plays.
 */
#ifdef __SDCC
#pragma bank 1
#endif
#include <string.h>
#include "compose.h"

Composer comp;

/* Scale offsets (semitones) for five degrees; base MIDI for degree 0. */
static const uint8_t scale_off[SCALE_COUNT][5] = {
    {0, 3, 5, 7, 10},  /* D minor pentatonic: D F G A C (SILT default) */
    {0, 2, 4, 7, 9},   /* C major pentatonic: C D E G A (SILT alternate) */
    {0, 7, 12, 14, 19},/* open fifths: D A D E A (bell-like, very open) */
    {0, 2, 5, 7, 9},   /* suspended: D E G A B */
};
static const uint8_t scale_base[SCALE_COUNT] = {50, 48, 50, 50};

/* Consonance against the drone, by semitone class. */
static const uint8_t consonance[12] = {8, 0, 3, 6, 6, 5, 0, 8, 4, 6, 3, 1};

/* Chord (drone degree) movement: from row -> weights to each degree. */
static const uint8_t chord_markov[5][5] = {
    {2, 3, 4, 5, 3}, {5, 1, 2, 3, 2}, {6, 2, 1, 3, 2}, {6, 2, 3, 1, 2}, {5, 2, 3, 3, 1},
};

typedef struct {
    uint8_t rest;      /* base steps between phrases */
    uint8_t chance;    /* 0..255 chance to begin a phrase when rest is over */
    uint8_t lo, hi;    /* melody degree range */
    uint8_t env;       /* NR12 for melody */
    uint8_t duty;
    uint8_t insect;    /* reply/call chance */
    uint8_t drone;     /* water level 0..3 */
    uint8_t ambience;  /* AMB_* */
    uint8_t bpm;
} Profile;

#define AMB_WIND  1
#define AMB_WATER 2
#define AMB_WOOD  4
#define AMB_CLOCK 8
#define AMB_NIGHT 16

static const Profile profiles[SCENE_COUNT] = {
    /* rest chance lo hi env  duty ins drone amb                  bpm */
    {  0,    0,  5, 12, 0x00, 2,   0, 0, 0,                        76}, /* SILENT */
    { 10,  200,  6, 13, 0x93, 2,  60, 1, AMB_WIND,                 72}, /* TITLE */
    { 20,  150,  5, 10, 0x73, 2,  20, 0, AMB_CLOCK,                76}, /* COTTAGE */
    { 14,  170,  6, 12, 0x83, 2,  70, 0, AMB_WIND,                 80}, /* VILLAGE */
    { 18,  140,  7, 13, 0x73, 1, 110, 0, AMB_WIND | AMB_WOOD,      72}, /* WOOD */
    { 16,  150,  6, 12, 0x83, 2,  80, 1, AMB_WATER,                76}, /* CREEK */
    { 48,   70,  5,  9, 0x53, 2,  10, 0, AMB_WIND,                 66}, /* RAILWAY: absence */
    { 14,  170,  8, 14, 0x83, 1, 120, 1, AMB_WIND,                 70}, /* HILL */
    { 64,   50,  8, 13, 0x43, 2,  30, 0, 0,                        60}, /* CLEARING */
    { 16,  150,  6, 12, 0x83, 2,  90, 1, AMB_WATER,                76}, /* POND SHORE */
    { 28,  110,  6, 13, 0x73, 2, 100, 2, AMB_WATER,                76}, /* POND (instrument) */
    { 30,  110,  6, 13, 0x73, 2, 100, 2, AMB_WATER,                70}, /* ENDING */
    { 40,   80,  5, 10, 0x53, 2, 140, 1, AMB_NIGHT,                66}, /* NIGHT */
};

/* Authored motifs: degrees (melody register, 5 = tonic) and gaps (steps). */
static const uint8_t motif_deg[MOTIF_COUNT][6] = {
    {8, 9, 10, 8, 0, 0},     /* POND: the first answer */
    {5, 8, 0, 0, 0, 0},      /* BELL: low and high, a fifth */
    {6, 7, 8, 7, 6, 5},      /* MAE: a lullaby she half-remembers */
    {10, 9, 8, 9, 10, 0},    /* IRIS */
    {9, 8, 0, 0, 0, 0},      /* TRAIN: two-tone whistle, remembered */
    {10, 13, 0, 0, 0, 0},    /* CHIME: the bell's fifth, an octave up */
    {10, 13, 12, 10, 8, 0},  /* ANSWER: the pond replies */
};
static const uint8_t motif_gap[MOTIF_COUNT][6] = {
    {4, 4, 8, 8, 0, 0}, {6, 8, 0, 0, 0, 0}, {3, 3, 6, 3, 3, 8},
    {2, 2, 2, 2, 4, 0}, {8, 8, 0, 0, 0, 0}, {5, 8, 0, 0, 0, 0}, {3, 3, 3, 6, 10, 0},
};
static const uint8_t motif_len[MOTIF_COUNT] = {4, 2, 6, 5, 2, 2, 5};

static const uint8_t wood_noise[5] = {0x2C, 0x3B, 0x3C, 0x4B, 0x4C};

static SndEv ev; /* shared scratch event (saves stack on the SM83) */

static uint16_t rnd(void) {
    uint16_t x = comp.rng;
    x ^= x << 7; x ^= x >> 9; x ^= x << 8;
    return comp.rng = x;
}
static uint16_t rnd_env(void) {
    uint16_t x = comp.rng_env;
    x ^= x << 7; x ^= x >> 9; x ^= x << 8;
    return comp.rng_env = x;
}
static uint8_t chance(uint8_t p) { return (uint8_t)(rnd() >> 8) < p; }
static uint8_t chance_env(uint8_t p) { return (uint8_t)(rnd_env() >> 8) < p; }

static uint8_t pick(const uint8_t *w, uint8_t n) {
    uint16_t sum = 0, r;
    uint8_t i;
    for (i = 0; i < n; ++i) sum += w[i];
    if (!sum) return 0;
    r = rnd() % sum;
    for (i = 0; i < n; ++i) { if (r < w[i]) return i; r -= w[i]; }
    return n - 1;
}

uint8_t compose_scale_midi(uint8_t degree) MB {
    uint8_t s = comp.scale < SCALE_COUNT ? comp.scale : 0;
    return (uint8_t)(scale_base[s] + 12 * (degree / 5) + scale_off[s][degree % 5]);
}

static uint8_t step_frames(void) { return (uint8_t)(comp.step_fp >> 8); }

static void emit(uint8_t ch, uint8_t note, uint8_t env, uint8_t timbre, uint8_t hold,
                 uint8_t prio, uint8_t fx, uint8_t fxarg, uint8_t pan, uint8_t delay) {
    ev.ch = ch; ev.note = note; ev.env = env; ev.timbre = timbre; ev.hold = hold;
    ev.prio = prio; ev.fx = fx; ev.fxarg = fxarg; ev.pan = pan;
    compose_emit(&ev, delay);
    ++comp.notes;
}

static uint8_t env_scaled(uint8_t env, int8_t dv) {
    int8_t v = (int8_t)(env >> 4) + dv;
    if (v < 1) v = 1;
    if (v > 15) v = 15;
    return (uint8_t)((v << 4) | (env & 0x0F));
}

static void echo_of(uint8_t ch, uint8_t note, uint8_t env, uint8_t duty, uint8_t delay) {
    uint8_t other, sf, pan;
    if (!comp.echo || ch > 1 || !(comp.behaviours & BEH_ECHO)) return;
    other = ch ? CH_PEBBLE : CH_LIVING;
    sf = step_frames();
    comp.echo_flip ^= 1;
    pan = comp.echo_flip ? PAN_L : PAN_R;
    if ((uint16_t)delay + 3u * sf < 250)
        emit(other, note, env_scaled(env, -5), duty, 3 * sf, PRIO_ECHO, 0, 0, pan, delay + 3 * sf);
    if (comp.echo == ECHO_DEEP && (uint16_t)delay + 6u * sf < 250)
        emit(other, note, env_scaled(env, -8), duty, 3 * sf, PRIO_ECHO, 0, 0,
             pan == PAN_L ? PAN_R : PAN_L, delay + 6 * sf);
}

/* Sound one lily-style note. vel: 0..15 relative loudness. */
static void tone(uint8_t type, uint8_t degree, uint8_t vel, uint8_t delay, uint8_t prio, uint8_t pan) {
    uint8_t note, env;
    switch (type) {
    case LILY_BELL:
        note = compose_scale_midi(degree);
        env = (uint8_t)((vel << 4) | 3);
        emit(CH_PEBBLE, note, env, 2, 40, prio, 0, 0, pan, delay);
        echo_of(CH_PEBBLE, note, env, 2, delay);
        break;
    case LILY_REED:
        note = compose_scale_midi(degree);
        env = (uint8_t)(((vel > 3 ? vel - 3 : 1) << 4) | 5);
        emit(CH_LIVING, note, env, 1, 50, prio, FX_VIB, 1, pan, delay);
        echo_of(CH_LIVING, note, env, 1, delay);
        break;
    case LILY_ROOT:
        note = (uint8_t)(compose_scale_midi(degree) - 12); /* SILT bass: 38..48 */
        emit(CH_WATER, note, 3, comp.wave, 48, prio, FX_WDECAY, 0, PAN_C, delay);
        break;
    default: /* LILY_WOOD: tonal noise, kept dry like SILT's Click */
        env = (uint8_t)(((vel > 2 ? vel - 2 : 1) << 4) | 1);
        emit(CH_ENV, wood_noise[degree % 5], env, 0, 8, prio, 0, 0, pan, delay);
        break;
    }
}

/* ---------- harmony ---------- */
static void drone_update(uint8_t delay) {
    const Profile *p = &profiles[comp.scene];
    uint8_t level = p->drone;
    if (!(comp.behaviours & BEH_WATER) || !comp.water) level = 0;
    if (comp.scene == SCENE_TITLE) level = 1;
    if (!level) {
        /* No water in this scene: make sure a drone from elsewhere stops.
           Sent on every scene change (cheap), because a new game resets the
           composer while the title's drone may still be sounding. */
        if (comp.drone_note || comp.step == 0 || delay == 0) {
            emit(CH_WATER, 50, 0, comp.wave, 0, PRIO_AMBIENT, FX_DRONE, 1, PAN_C, delay);
            comp.drone_note = 0;
        }
        return;
    }
    if (comp.hush && level > 1) level = 1;
    comp.drone_note = compose_scale_midi(comp.chord);
    while (comp.drone_note > 50) comp.drone_note -= 12;
    emit(CH_WATER, comp.drone_note, level, comp.wave, 0, PRIO_AMBIENT, FX_DRONE,
         (comp.behaviours & BEH_WIND) && comp.drift ? 1 : 2, PAN_C, delay);
}

static void harmony_step(uint8_t delay) {
    if (comp.hold) return;
    if (comp.chord_left) { --comp.chord_left; return; }
    comp.chord = pick(chord_markov[comp.chord % 5], 5);
    comp.chord_left = (uint8_t)(32 + 16 * (rnd() % 3));
    if (comp.density == DENS_STILL) comp.chord_left += 32;
    drone_update(delay);
}

static uint8_t consonant(uint8_t degree) {
    uint8_t m = compose_scale_midi(degree);
    uint8_t d = compose_scale_midi(comp.chord);
    while (m < d) m += 12;
    return consonance[(uint8_t)(m - d) % 12];
}

/* ---------- melody (pebbles) ---------- */
static uint8_t next_degree(uint8_t from, uint8_t lo, uint8_t hi) {
    static const int8_t moves[8] = {0, 1, -1, 2, -2, 3, -3, 5};
    static const uint8_t base[8] = {2, 8, 8, 5, 5, 3, 3, 2};
    uint8_t w[8], i, mem;
    for (i = 0; i < 8; ++i) {
        int8_t d = (int8_t)from + moves[i];
        uint8_t a = (uint8_t)(moves[i] < 0 ? -moves[i] : moves[i]);
        if (d < (int8_t)lo || d > (int8_t)hi) { w[i] = 0; continue; }
        mem = a < 5 ? mmem_interval_weight(a) : 0;
        if (!(comp.behaviours & BEH_MEMORY)) mem >>= 2;
        w[i] = (uint8_t)((base[i] + mem) * (1 + consonant((uint8_t)d)) / 3);
        if (!w[i]) w[i] = 1;
    }
    i = pick(w, 8);
    return (uint8_t)((int8_t)from + moves[i]);
}

static uint8_t rhythm_gap(void) {
    static const uint8_t gaps[4] = {2, 3, 4, 6};
    uint8_t w[4], i;
    for (i = 0; i < 4; ++i) {
        w[i] = (uint8_t)(4 + ((comp.behaviours & BEH_MEMORY) ? mmem_rhythm_gap(i) : 0));
    }
    if (comp.density <= DENS_SPARSE) { w[0] = 1; w[3] += 4; }
    if (comp.density == DENS_ACTIVE) { w[0] += 3; }
    return gaps[pick(w, 4)];
}

static void store_phrase(void) {
    if (comp.cur.len < 2) return;
    comp.bank[comp.bank_next & 3] = comp.cur;
    comp.bank_next = (uint8_t)((comp.bank_next + 1) & 3);
}

static uint8_t rest_length(void) {
    static const uint8_t mult[DENS_COUNT] = {4, 2, 1, 1};
    const Profile *p = &profiles[comp.scene];
    uint8_t tide = compose_tide();
    uint16_t r = (uint16_t)p->rest * mult[comp.density % DENS_COUNT];
    if (comp.density == DENS_ACTIVE) r >>= 1;
    r += (uint8_t)(255 - tide) >> 4;
    r += rnd() % (r + 1);
    if (comp.behaviours & BEH_STILL && comp.density == DENS_STILL) r += 24;
    return r > 250 ? 250 : (uint8_t)r;
}

static void begin_phrase(void) {
    const Profile *p = &profiles[comp.scene];
    uint8_t recall_p = (comp.behaviours & BEH_MEMORY) ? 70 : 20;
    comp.cur.len = 0;
    comp.recalling = 0;
    if (comp.bank[rnd() & 3].len && chance(recall_p)) {
        uint8_t i;
        int8_t shift;
        comp.recall = comp.bank[rnd() & 3];
        if (comp.recall.len) {
            /* Repetition with controlled variation. */
            shift = (int8_t)(rnd() % 3) - 1;
            for (i = 0; i < comp.recall.len; ++i) {
                int8_t d = (int8_t)comp.recall.deg[i] + shift;
                if (d < (int8_t)p->lo) d += 5;
                if (d > (int8_t)p->hi) d -= 5;
                comp.recall.deg[i] = (uint8_t)d;
            }
            if (chance(80)) {
                i = (uint8_t)(rnd() % comp.recall.len);
                comp.recall.deg[i] = next_degree(comp.recall.deg[i], p->lo, p->hi);
            }
            if (chance(60)) for (i = 0; i < comp.recall.len; ++i) comp.recall.gap[i] <<= 1;
            comp.recalling = 1;
            comp.recall_pos = 0;
            comp.phrase_left = comp.recall.len;
            return;
        }
    }
    comp.phrase_left = (uint8_t)(2 + rnd() % (comp.density >= DENS_GENTLE ? 4 : 3));
    /* Start on a consonant degree near the last one. */
    {
        uint8_t tries = 4, d = comp.mel_deg;
        if (d < p->lo || d > p->hi) d = (uint8_t)((p->lo + p->hi) >> 1);
        while (tries-- && consonant(d) < 5) d = next_degree(d, p->lo, p->hi);
        comp.mel_deg = d;
    }
}

static void melody_step(uint8_t delay) {
    const Profile *p = &profiles[comp.scene];
    uint8_t deg, vel, gap;
    if (!p->chance || comp.quiet) return;
    if (comp.note_wait) { --comp.note_wait; return; }
    if (!comp.phrase_left) {
        if (comp.rest_left) { --comp.rest_left; ++comp.rests; return; }
        if (comp.hush) { --comp.hush; return; }
        if (!chance(p->chance)) { comp.rest_left = 2; return; }
        begin_phrase();
    }
    if (comp.recalling) {
        deg = comp.recall.deg[comp.recall_pos];
        gap = comp.recall.gap[comp.recall_pos];
        ++comp.recall_pos;
    } else {
        deg = comp.cur.len ? next_degree(comp.mel_deg, p->lo, p->hi) : comp.mel_deg;
        if (comp.phrase_left == 1) {
            /* Phrase ending leans toward a consonance. */
            uint8_t alt = next_degree(deg, p->lo, p->hi);
            if (consonant(alt) > consonant(deg)) deg = alt;
        }
        gap = rhythm_gap();
        if (comp.cur.len < 6) {
            comp.cur.deg[comp.cur.len] = deg;
            comp.cur.gap[comp.cur.len] = gap;
            ++comp.cur.len;
        }
    }
    comp.mel_deg = deg;
    vel = (uint8_t)(p->env >> 4);
    if (comp.cur.len == 1 || comp.recall_pos == 1) vel += 1;
    if (vel > 13) vel = 13;
    if (comp.density == DENS_STILL && vel > 4) vel -= 2;
    ev.timbre = p->duty;
    {
        uint8_t note = compose_scale_midi(deg);
        uint8_t env = (uint8_t)((vel << 4) | (p->env & 7));
        emit(CH_PEBBLE, note, env, p->duty, 30, PRIO_MUSIC, 0, 0, PAN_C, delay);
        echo_of(CH_PEBBLE, note, env, p->duty, delay);
    }
    if ((comp.behaviours & BEH_INSECTS) && comp.insects && !comp.reply_in &&
        chance(p->insect) && deg < 12) {
        /* Acoustic niche: insects answer, but leave the high register free
           while the melody occupies it. */
        static const int8_t rep[5] = {0, 2, 3, 5, -2};
        static const uint8_t rw[5] = {3, 4, 4, 3, 2};
        comp.reply_deg = (uint8_t)((int8_t)deg + rep[pick(rw, 5)]);
        comp.reply_in = (uint8_t)(2 + rnd() % 4);
    }
    comp.note_wait = gap - 1;
    if (--comp.phrase_left == 0) {
        if (!comp.recalling) store_phrase();
        comp.rest_left = rest_length();
    }
}

/* ---------- living sounds ---------- */
static void insect_chirp(uint8_t degree, uint8_t delay, uint8_t prio) {
    uint8_t note = (uint8_t)(compose_scale_midi(degree) + 12);
    uint8_t reps = (uint8_t)(2 + (rnd() & 3));
    while (note > 98) note -= 12;
    comp.echo_flip ^= 1;
    emit(CH_LIVING, note, 0x61, 0, (uint8_t)(reps * 3 + 4), prio, FX_TRILL,
         (uint8_t)((reps << 4) | 3), comp.echo_flip ? PAN_L : PAN_R, delay);
}

static void living_step(uint8_t delay) {
    const Profile *p = &profiles[comp.scene];
    if (!(comp.behaviours & BEH_INSECTS) || !comp.insects) return;
    if (comp.reply_in) {
        if (--comp.reply_in == 0 && !comp.hush) {
            insect_chirp(comp.reply_deg, delay, PRIO_MUSIC);
        }
        return;
    }
    /* Their own sparse calls, more at night and when the pond is quiet. */
    if (comp.phrase_left == 0 && !comp.hush) {
        uint8_t c = p->insect >> 4;
        if (p->ambience & AMB_NIGHT) c <<= 1;
        if (comp.density == DENS_STILL) c >>= 1;
        c += mmem_voice_bias(LILY_REED) >> 2;
        if (chance(c)) insect_chirp((uint8_t)(10 + (rnd() % 4)), delay, PRIO_AMBIENT);
    }
}

/* ---------- environment ---------- */
static void env_step(uint8_t delay) {
    const Profile *p = &profiles[comp.scene];
    uint8_t amb = p->ambience, sub;
    uint8_t rain = comp.rain;
    if (!(comp.behaviours & BEH_RAIN) && comp.scene == SCENE_POND) rain = 0;
    if (comp.weather > rain) rain = comp.weather;   /* the day's weather is always heard */
    if (rain) {
        static const uint8_t rain_p[4] = {0, 40, 110, 200};
        uint8_t n = rain >= RAIN_SHOWER ? 2 : 1;
        while (n--) {
            if (chance_env(rain_p[rain])) {
                sub = (uint8_t)(rnd_env() % step_frames());
                emit(CH_ENV, (uint8_t)(0x10 + (rnd_env() & 0x13)),
                     (uint8_t)(((2 + (rnd_env() & 3)) << 4) | 1), 0, 3, PRIO_AMBIENT, 0, 0,
                     (uint8_t)(rnd_env() % 3), delay + sub);
            }
        }
    }
    if ((amb & AMB_CLOCK) && (comp.step & 7) == 0) {
        emit(CH_ENV, (comp.step & 8) ? 0x2A : 0x29, 0x21, 0, 3, PRIO_AMBIENT, 0, 0, PAN_C, delay);
    }
    if ((amb & AMB_WIND) && (comp.behaviours & BEH_WIND) && (comp.step & 3) == 0 &&
        chance_env(18)) {
        /* A soft gust: long, low noise that rises into the decay. */
        emit(CH_ENV, (uint8_t)(0x56 + (rnd_env() & 0x11)), 0x37, 0, 40, PRIO_AMBIENT, 0, 0,
             PAN_C, delay);
    }
    if ((amb & AMB_WATER) && chance_env(comp.scene == SCENE_CREEK ? 70 : 25)) {
        sub = (uint8_t)(rnd_env() % step_frames());
        emit(CH_ENV, (uint8_t)(0x22 + (rnd_env() & 0x11)), 0x11, 0, 2, PRIO_AMBIENT, 0, 0,
             PAN_C, delay + sub);
    }
    if ((amb & AMB_WOOD) && (comp.behaviours & BEH_WOOD) && chance_env(10)) {
        tone(LILY_WOOD, (uint8_t)(rnd_env() % 5), 6, delay, PRIO_AMBIENT, PAN_C);
    }
}

/* ---------- time ---------- */
uint8_t compose_tide(void) MB {
    uint16_t a = comp.tide_a & 0x3FF, b = comp.tide_b;
    uint8_t ta, tb;
    ta = (uint8_t)((a & 0x200 ? 0x3FF - a : a) >> 1);
    tb = (uint8_t)((b >= 768 ? 1535 - b : b) / 3);
    return (uint8_t)(((uint16_t)ta * 3 + (uint16_t)tb * 5) >> 3);
}

static void do_step(uint8_t delay) {
    comp.tide_a++;
    if (++comp.tide_b >= 1536) comp.tide_b = 0;
    if (!comp.hush && !comp.phrase_left && chance(2)) {
        comp.hush = (uint8_t)(16 + (rnd() & 31));
        if (comp.behaviours & BEH_STILL) comp.hush += 16;
    }
    if (comp.scene != SCENE_SILENT) {
        harmony_step(delay);
        if (comp.scene == SCENE_POND || comp.scene == SCENE_ENDING) garden_step();
        melody_step(delay);
        living_step(delay);
        env_step(delay);
    }
    ++comp.step;
}

static uint8_t g_delay; /* delay of the step being processed (garden uses it) */

void compose_frame(void) MB {
    uint16_t now = compose_now();
    int16_t ahead;
    uint8_t guard = 0;
    /* Never burst missed notes after a long interruption. */
    if ((int16_t)(now - comp.next_at) > 24) { comp.next_at = now; comp.next_frac = 0; }
    for (;;) {
        ahead = (int16_t)(comp.next_at - now);
        if (ahead > 8 || guard++ > 2) break;
        g_delay = ahead < 0 ? 0 : (uint8_t)ahead;
        do_step(g_delay);
        {
            uint16_t f = (uint16_t)comp.next_frac + (comp.step_fp & 0xFF);
            comp.next_at += (comp.step_fp >> 8) + (f >> 8);
            comp.next_frac = (uint8_t)f;
        }
    }
}

/* Called by the garden when a wave crosses a lily (player = 0) and by the
   pond interface for direct notes (player = 1). */
void compose_player_note(uint8_t type, uint8_t row, uint8_t col, uint8_t player) MB {
    uint8_t degree, vel, pan;
    if (type == LILY_NONE || type >= LILY_COUNT || row >= POND_ROWS) return;
    if (!player && comp.density == DENS_STILL && !chance(150)) return;
    if (!player && comp.hush && !chance(170)) return;
    degree = (uint8_t)(4 - row);
    if (type == LILY_BELL) degree += 10;
    else if (type == LILY_REED) degree += 5;
    vel = player ? 12 : (uint8_t)(8 + (mmem_voice_bias(type) >> 3));
    if (comp.density == DENS_STILL) vel -= 2;
    pan = col < 3 ? PAN_L : (col > 4 ? PAN_R : PAN_C);
    tone(type, degree, vel, player ? 0 : g_delay, player ? PRIO_PLAYER : PRIO_MUSIC, pan);
    if (type != LILY_WOOD && (comp.behaviours & BEH_INSECTS) && comp.insects &&
        !comp.reply_in && chance(player ? 140 : 50)) {
        comp.reply_deg = (uint8_t)(degree % 5 + 10);
        comp.reply_in = (uint8_t)(3 + (rnd() & 3));
    }
    if (player) {
        mmem_player_note(type, row);
        if (comp.cur.len < 6) {
            comp.cur.deg[comp.cur.len] = (uint8_t)(degree % 5 + 5);
            comp.cur.gap[comp.cur.len] = 4;
            if (++comp.cur.len == 4) { store_phrase(); comp.cur.len = 0; }
        }
    }
}

void compose_motif(uint8_t motif, uint8_t delay) MB {
    uint8_t i, d = delay, sf = step_frames();
    if (motif >= MOTIF_COUNT) return;
    for (i = 0; i < motif_len[motif]; ++i) {
        uint8_t note = compose_scale_midi(motif_deg[motif][i]);
        if (motif == MOTIF_CHIME) note += 12;
        emit(motif == MOTIF_CHIME ? CH_LIVING : CH_PEBBLE, note,
             motif == MOTIF_TRAIN ? 0x87 : 0xA4, motif == MOTIF_TRAIN ? 1 : 2, 40, PRIO_MOTIF,
             motif == MOTIF_TRAIN ? FX_VIB : 0, 2, PAN_C, d);
        if ((uint16_t)d + (uint16_t)motif_gap[motif][i] * sf > 250) break;
        d += motif_gap[motif][i] * sf;
    }
    /* A heard motif enters the pond's phrase memory. */
    mmem.motifs |= (uint8_t)(1u << motif);
    {
        Phrase *ph = &comp.bank[comp.bank_next & 3];
        ph->len = motif_len[motif];
        for (i = 0; i < ph->len; ++i) { ph->deg[i] = motif_deg[motif][i]; ph->gap[i] = motif_gap[motif][i]; }
        comp.bank_next = (uint8_t)((comp.bank_next + 1) & 3);
    }
    comp.note_wait = (uint8_t)(d / (sf ? sf : 1) + 4);
}

void compose_hush(uint8_t steps) MB { comp.hush = steps; comp.phrase_left = 0; }

void compose_set_tempo(uint8_t bpm) MB {
    if (bpm < 50) bpm = 50;
    if (bpm > 140) bpm = 140;
    comp.bpm = bpm;
    /* audio ticks per step, 8.8: 60.24 Hz timer * 60 s * 256 / (4 steps per beat) */
    comp.step_fp = (uint16_t)(231322UL / bpm);
}

void compose_apply_pond(void) MB {
    comp.scale = pond.scale % SCALE_COUNT;
    comp.density = pond.density % DENS_COUNT;
    comp.rain = pond.rain % RAIN_COUNT;
    comp.echo = pond.echo % ECHO_COUNT;
    comp.drift = pond.drift & 1;
    comp.water = pond.water & 1;
    comp.insects = pond.insects & 1;
    comp.hold = pond.hold & 1;
    comp.wave = pond.wave % WAVE_COUNT;
    compose_set_tempo(pond.bpm ? pond.bpm : 76);
    drone_update(0);
}

void compose_scene(uint8_t scene, uint8_t behaviours) MB {
    const Profile *p;
    if (scene >= SCENE_COUNT) scene = SCENE_SILENT;
    comp.scene = scene;
    comp.behaviours = behaviours;
    p = &profiles[scene];
    comp.phrase_left = 0;
    comp.rest_left = 4;
    comp.note_wait = 0;
    comp.hush = 0;
    comp.reply_in = 0;
    comp.quiet = 0;
    comp.weather = 0;
    if (scene == SCENE_POND || scene == SCENE_ENDING) {
        compose_apply_pond();
        if (scene == SCENE_ENDING) compose_set_tempo((uint8_t)(comp.bpm - 6));
        return;
    }
    /* Overworld: the world's own settings */
    comp.scale = scene == SCENE_HILL ? SCALE_FIFTHS : SCALE_DMINOR;
    comp.density = DENS_SPARSE;
    comp.echo = (behaviours & BEH_ECHO) && (scene == SCENE_RAILWAY || scene == SCENE_CLEARING) ? ECHO_MIST : ECHO_DRY;
    comp.drift = 1;
    comp.water = 1;
    comp.insects = 1;
    comp.hold = 0;
    comp.wave = scene == SCENE_WOOD ? WAVE_HOLLOW : WAVE_SOFT;
    compose_set_tempo(p->bpm);
    drone_update(0);
}

void compose_init(uint16_t seed) MB {
    memset(&comp, 0, sizeof(comp));
    comp.rng = seed ? seed : 0x5113;
    comp.rng_env = (uint16_t)(seed * 31u + 0x7261) | 1;
    comp.mel_deg = 8;
    comp.chord_left = 16;
    comp.next_at = compose_now() + 2;
    compose_set_tempo(76);
}
