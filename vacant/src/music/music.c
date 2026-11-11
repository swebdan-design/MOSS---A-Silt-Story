/* VACANT - generative ambient composer (home bank, runs inside VBlank).
 *
 * Time:      frames -> steps (SPEED) -> bars of 16 steps -> phrases of 4 bars.
 * Harmony:   a tonal centre (ROOT + slow drift around the circle of fifths),
 *            a mode, and a chord degree that moves by weighted random walk
 *            every HARM bars unless FREEZE HARMONY is on.
 * Roles:     DRONE  -> CH3 wave, glides between chord roots, slow detune drift
 *            MELODY -> CH1, motifs of 8 notes stated, varied and mutated
 *            PULSE  -> CH2, arpeggio / mechanism / shimmer / intervals
 *            NOISE  -> CH4, rain, wind, machinery, thunder
 *            echoes, landmarks and city events borrow channels by priority.
 * Evolution: micro  - every note: velocity, duty, length, vibrato
 *            meso   - every phrase: motif choice and mutation, pan, breaths
 *            macro  - every few phrases (STABILITY): mode, register, timbre,
 *                     texture and tonal-centre drift, new motifs
 * Place:     district profiles and weather move "target" values; the values
 *            the music actually uses glide toward them one unit per bar.
 */
#include "music.h"
#include "../audio/sound.h"

uint8_t mparam[NPARAM];
const uint8_t param_max[NPARAM] = {15, 15, 15, 15, 11, 7, 15, 15, 15, 15, 15, 15, 15, 15, 3, 4, 8, 2, 3};
uint16_t music_seed;
uint8_t music_freeze, music_focus, music_rain, music_fog, music_storm;
volatile uint16_t music_steps;
volatile uint8_t music_cur_root, music_cur_mode, music_cur_degree, music_section;

/* --------------------------------------------------------------- presets */
static const uint8_t presets[PRESET_COUNT][NPARAM] = {
    /* drn mel pul noi root mode brt ten den mut spd stb atm inf sty wav hrm ech mot */
    {11, 10,  8,  6,  9,  0,  7,  4,  6,  5,  6, 10,  8, 10,  0,  0,  4,  1,  1}, /* EMPTY STREETS */
    {12,  6, 11, 10,  4,  2,  6,  8, 10,  3,  9, 12, 10, 10,  1,  4,  8,  0,  0}, /* THE WORKS     */
    {12,  7,  6,  5,  1,  7,  2, 11,  3,  6,  3,  9,  6, 12,  3,  2,  8,  2,  2}, /* LOWER CITY    */
    { 9, 10, 10,  6,  2,  1,  9,  5,  8,  7,  8,  6,  9,  9,  2,  1,  2,  1,  1}, /* NIGHT SERVICE */
    { 8, 11,  7,  3,  5,  6, 12,  2,  4,  8,  4,  4,  5,  8,  0,  3,  4,  2,  2}, /* BEFORE DAWN   */
};
static const uint16_t preset_seed[PRESET_COUNT] = {0x7A31u, 0x0BEDu, 0x51C4u, 0x24F7u, 0x6D02u};
static const char *const preset_names[PRESET_COUNT] = {
    "EMPTY STREETS", "THE WORKS", "LOWER CITY", "NIGHT SERVICE", "BEFORE DAWN"};

/* ----------------------------------------------------------------- modes */
static const uint8_t scale_len[MODE_COUNT] = {7, 7, 7, 5, 6, 5, 7, 7};
static const uint8_t scale_tab[MODE_COUNT][7] = {
    {0, 2, 3, 5, 7, 8, 10},   /* AEOLIAN    natural minor */
    {0, 2, 3, 5, 7, 9, 10},   /* DORIAN                   */
    {0, 1, 3, 5, 7, 8, 10},   /* PHRYGIAN                 */
    {0, 3, 5, 7, 10, 0, 0},   /* PENTATONIC minor         */
    {0, 2, 4, 6, 8, 10, 0},   /* WHOLE TONE               */
    {0, 2, 5, 7, 9, 0, 0},    /* SUSPENDED  ambient set   */
    {0, 2, 4, 6, 7, 9, 11},   /* LYDIAN                   */
    {0, 1, 3, 5, 6, 8, 10},   /* LOCRIAN                  */
};
static const char *const mode_names[MODE_COUNT] = {
    "AEOLIAN", "DORIAN", "PHRYGIAN", "PENTATONIC", "WHOLE TONE", "SUSPENDED", "LYDIAN", "LOCRIAN"};
/* neighbouring mode for slow drift (by brightness) */
static const uint8_t mode_drift[MODE_COUNT] = {1, 0, 0, 5, 6, 3, 1, 2};

/* -------------------------------------------------- district profiles */
typedef struct {
    int8_t dens, noise, reg, melp, env, root, tens;
    uint8_t style;    /* 0xFF keep */
    uint8_t penta;    /* melody restricted to a pentatonic subset */
} Profile;
static const Profile prof[5] = {
    /* dens noise  reg melp env root tens style penta */
    {  0,   0,    0,   9,   2,   0,   0, 0xFF, 0},   /* EMPTY STREETS */
    {  3,   4,  -12,   3,   0,   5,   2,    1, 0},   /* INDUSTRIAL    */
    { -1,  -2,   12,  11,   2,   3,  -2,    0, 0},   /* RESIDENTIAL   */
    { -3,   0,  -12,   4,   3,   1,   4,    3, 0},   /* LOWER CITY    */
    { -3,  -4,   12,   6,   3,   7,  -3, 0xFF, 1},   /* FORGOTTEN     */
};

/* ---------------------------------------------------------------- state */
#define REST (-128)
#define NMOT 4u
#define MLEN 8u
static int8_t mot_deg[NMOT][MLEN];
static uint8_t mot_len[NMOT][MLEN];

static uint16_t mrs = 1;
static uint8_t frames, step_frames, step, bar, phrase;
static uint8_t chord_deg, chord_left, macro_left;
static int8_t fifths;             /* tonal-centre drift, -2..2 fifths */
static uint8_t cur_mode, root;
static int8_t reg_drift;          /* -12, 0, 12 */
static uint8_t wave_off, style_swap, breath;

static uint8_t district, lm_type, lm_gain;
/* effective (smoothed) values */
static uint8_t e_dens, e_noise, e_bright, e_tens, e_melp, e_env;
static int8_t e_reg, e_root_off;

/* melody player */
static uint8_t mel_motif, mel_i, mel_wait, mel_form, mel_relchord, mel_pan;
static uint8_t vib_t, vib_on;
static uint16_t mel_per;
/* pulse player */
static uint8_t pu_k, pu_dir, pu_pan;
static uint8_t sh_on, sh_t, sh_i;
static uint16_t sh_per[3];
/* drone */
static uint8_t drone_note, drone_level, drone_wave, drone_lfo;
static uint16_t drone_per, drone_target;
/* channel arbitration */
static uint8_t busy[4], prio[4];
/* echoes */
typedef struct { uint8_t t, note, env, pan; } Echo;
static Echo echo[4];

static const uint8_t forms[8][4] = {
    {0, 0, 1, 0}, {0, 1, 0, 2}, {0, 0, 0, 1}, {0, 1, 2, 1},
    {0, 2, 0, 3}, {1, 1, 0, 0}, {0, 3, 0, 1}, {2, 0, 2, 1}};
static const int8_t vib_shape[8] = {0, 1, 2, 1, 0, -1, -2, -1};

uint8_t music_rand(void) {
    mrs ^= mrs << 7;
    mrs ^= mrs >> 9;
    mrs ^= mrs << 8;
    return (uint8_t)mrs;
}
#define RND() music_rand()
static uint8_t rn(uint8_t n) { return (uint8_t)(((uint16_t)RND() * n) >> 8); }

const char *music_mode_name(uint8_t m) { return mode_names[m & 7]; }
const char *music_preset_name(uint8_t n) { return preset_names[n % PRESET_COUNT]; }

static uint8_t clamp15(int8_t v) { return v < 0 ? 0 : v > 15 ? 15 : (uint8_t)v; }

/* scale degree (any octave) -> semitones from the tonic */
static int8_t deg_semi(int8_t d) {
    uint8_t len = scale_len[cur_mode];
    int8_t oct = 0;
    while (d < 0) { d += (int8_t)len; oct -= 12; }
    while (d >= (int8_t)len) { d -= (int8_t)len; oct += 12; }
    return oct + (int8_t)scale_tab[cur_mode][(uint8_t)d];
}

static uint8_t clamp_note(int16_t n) {
    if (n < 36) n += 12;
    if (n < 36) n = 36;
    while (n > 103) n -= 12;
    return (uint8_t)n;
}

/* scale a mix level 0..15 into an envelope volume */
static uint8_t mixvol(uint8_t v, uint8_t mix) {
    if (!mix) return 0;
    v = (uint8_t)(((uint16_t)v * (mix + 1) + 15) >> 4);
    if (music_focus) v >>= 1;
    return v > 15 ? 15 : v;
}

static uint8_t claim(uint8_t ch, uint8_t p, uint8_t frames_) {
    if (busy[ch] && p < prio[ch]) return 0;
    busy[ch] = frames_;
    prio[ch] = p;
    return 1;
}

/* ---------------------------------------------------------------- motifs */
static void make_motif(uint8_t m) {
    uint8_t i;
    int8_t d = (int8_t)rn(5);          /* start near the tonic, upward bias */
    for (i = 0; i < MLEN; ++i) {
        uint8_t r = RND();
        if (i > 0 && r < 50) {
            mot_deg[m][i] = REST;
        } else {
            /* mostly steps, some thirds, rare leaps; clear weather leaps more */
            uint8_t k = RND();
            int8_t mv = k < 90 ? 1 : k < 150 ? 2 : k < 190 ? 0 : k < 225 ? 3 : 4;
            if (RND() & 1) mv = -mv;
            d += mv;
            if (d < -3) d = -1;
            if (d > 9) d = 6;
            mot_deg[m][i] = d;
        }
        mot_len[m][i] = (uint8_t)(1 + rn(4) + (RND() < 60 ? 3 : 0));
    }
    mot_len[m][0] += 1;
}

static void mutate_motif(uint8_t m) {
    uint8_t i = rn(MLEN), k = RND();
    if (k < 110) {
        if (mot_deg[m][i] != REST) mot_deg[m][i] += (RND() & 1) ? 1 : -1;
    } else if (k < 170) {
        mot_deg[m][i] = (mot_deg[m][i] == REST) ? (int8_t)rn(7) : REST;
    } else if (k < 220) {
        mot_len[m][i] = (uint8_t)(1 + rn(6));
    } else {
        uint8_t j = rn(MLEN);
        int8_t t = mot_deg[m][i];
        mot_deg[m][i] = mot_deg[m][j];
        mot_deg[m][j] = t;
    }
    if (mot_deg[m][0] == REST) mot_deg[m][0] = 0;
}

/* --------------------------------------------------------------- harmony */
static const int8_t chord_moves[8] = {3, -3, 4, -4, 2, -2, 1, -1};

static void next_chord(void) {
    uint8_t k = RND(), i;
    int8_t mv;
    /* consonant moves (4ths, 5ths, 3rds) unless tension asks for steps */
    if (k < (uint8_t)(150 - e_tens * 6)) i = rn(4);
    else if (k < 230) i = 4 + rn(2);
    else i = 6 + rn(2);
    mv = chord_moves[i];
    /* gravity back toward the tonic */
    if (chord_deg != 0 && RND() < 70) {
        chord_deg = 0;
    } else {
        int8_t d = (int8_t)chord_deg + mv;
        uint8_t len = scale_len[cur_mode];
        while (d < 0) d += (int8_t)len;
        while (d >= (int8_t)len) d -= (int8_t)len;
        chord_deg = (uint8_t)d;
    }
    music_cur_degree = chord_deg;
}

static uint8_t norm12(int16_t v) {
    while (v < 0) v += 12;
    while (v >= 12) v -= 12;
    return (uint8_t)v;
}

static void set_root(void) {
    int16_t r = (int16_t)mparam[P_ROOT] + e_root_off + fifths * 7;
    root = norm12(r);
    music_cur_root = root;
}

/* ----------------------------------------------------------------- drone */
static uint8_t drone_lvl_target(void) {
    uint8_t m = mparam[P_DRONE];
    /* the wave channel is strong: 25% for most settings, 50% high, 100% at max */
    uint8_t l = m == 0 ? 0 : m < 9 ? 1 : m < 15 ? 2 : 3;
    if (music_focus && l > 1) --l;
    return l;
}

static void drone_retune(uint8_t restart) {
    int8_t s = deg_semi((int8_t)chord_deg);
    uint8_t n;
    if (RND() < 50) s = deg_semi((int8_t)chord_deg + (scale_len[cur_mode] > 5 ? 4 : 3));
    n = (uint8_t)(36 + norm12((int16_t)root + s));
    if (e_reg < 0 && n > 40) n -= 12;
    drone_note = n;
    drone_target = snd_period((uint8_t)(n + 12));
    drone_level = drone_lvl_target();
    drone_wave = (uint8_t)((mparam[P_WAVE] + wave_off) % 5u);
    if (restart || drone_wave != snd_wave_loaded) {
        drone_per = drone_target;
        if (drone_level) snd_wave_start(n, drone_wave, drone_level, PAN_C);
        else snd_wave_level(0);
    } else {
        snd_wave_level(drone_level);
    }
}

static void drone_frame(void) {
    int8_t off;
    if (!drone_level) return;
    if (drone_per != drone_target) {          /* portamento */
        uint8_t rate = 1 + mparam[P_MOTION];
        if (drone_per < drone_target) { drone_per += rate; if (drone_per > drone_target) drone_per = drone_target; }
        else { if (drone_per - drone_target < rate) drone_per = drone_target; else drone_per -= rate; }
    }
    ++drone_lfo;
    off = (int8_t)(vib_shape[(drone_lfo >> 5) & 7] * (int8_t)mparam[P_MOTION]) >> 1;
    if ((drone_lfo & 3) == 0) snd_wave_period((uint16_t)((int16_t)drone_per + off));
}

/* ----------------------------------------------------------------- echo */
static void echo_push(uint8_t delay, uint8_t note, uint8_t env, uint8_t pan) {
    uint8_t i;
    if ((env >> 4) == 0) return;
    for (i = 0; i < 4; ++i)
        if (!echo[i].t) { echo[i].t = delay; echo[i].note = note; echo[i].env = env; echo[i].pan = pan; return; }
}

static void echo_frame(void) {
    uint8_t i;
    for (i = 0; i < 4; ++i) {
        if (echo[i].t && --echo[i].t == 0) {
            if (claim(1, 1, step_frames)) snd_pulse(1, echo[i].note, echo[i].env, 2, echo[i].pan, 0);
        }
    }
}

/* --------------------------------------------------------------- melody */
static uint8_t duty_of(void) {
    if (e_bright < 5) return 2;
    if (e_bright < 11) return (RND() & 1) ? 1 : 2;
    return (RND() & 1) ? 0 : 1;
}

static void melody_note(int8_t deg) {
    int16_t n;
    uint8_t vol, env, per, note, pan;
    if (prof[district].penta && mparam[P_INFLUENCE] > 4 && scale_len[cur_mode] == 7) {
        /* pentatonic filter: skip the 2nd and 6th degrees of the mode */
        int8_t d = deg;
        while (d < 0) d += 7;
        while (d >= 7) d -= 7;
        if (d == 1 || d == 5) deg += 1;
    }
    if (mel_relchord) deg += (int8_t)chord_deg;
    n = 60 + root + deg_semi(deg) + e_reg + reg_drift + (e_bright >= 11 ? 12 : 0);
    /* tension: occasional chromatic neighbour */
    if (e_tens > 8 && RND() < (uint8_t)(e_tens * 3)) n += (RND() & 1) ? 1 : -1;
    while (n > 91) n -= 12;      /* keep the melody below a piercing register */
    note = clamp_note(n);
    vol = mixvol((uint8_t)(10 + rn(5)), mparam[P_MELODY]);
    if (!vol) return;
    per = (uint8_t)(2 + e_env + (e_dens < 5 ? 1 : 0));
    if (per > 7) per = 7;
    env = (uint8_t)((vol << 4) | per);
    pan = mel_pan;
    if (!claim(0, 2, (uint8_t)(step_frames * 2))) return;
    snd_pulse(0, note, env, duty_of(), pan, 0);
    mel_per = snd_period(note);
    vib_t = 0;
    vib_on = mparam[P_MOTION];
    if (mparam[P_ECHO]) {
        uint8_t d = (uint8_t)(step_frames * 3);
        uint8_t ep = pan == PAN_L ? PAN_R : pan == PAN_R ? PAN_L : (RND() & 1) ? PAN_L : PAN_R;
        echo_push(d, note, (uint8_t)(((vol >> 1) << 4) | per), ep);
        if (mparam[P_ECHO] > 1) echo_push((uint8_t)(d * 2), note, (uint8_t)(((vol >> 2) << 4) | per), pan);
    }
}

static void melody_frame(void) {
    if (!vib_on || busy[0] == 0 || prio[0] != 2) return;
    if (++vib_t < 14) return;
    if ((vib_t & 1) == 0)
        snd_pulse_period(0, (uint16_t)((int16_t)mel_per + vib_shape[(vib_t >> 1) & 7] * (int8_t)vib_on / 2));
}

static void melody_step(void) {
    int8_t d;
    if (breath) return;
    if (mel_wait) { --mel_wait; return; }
    d = mot_deg[mel_motif][mel_i];
    if (d != REST && RND() < (uint8_t)(e_melp * 16 + 15)) melody_note(d);
    mel_wait = (uint8_t)(mot_len[mel_motif][mel_i] - 1);
    if (++mel_i >= MLEN) {
        /* statement complete: a silence, then the next motif in the form */
        mel_i = 0;
        mel_wait += (uint8_t)(4 + rn(8) + (15 - e_dens));
        mel_form = (mel_form + 1) & 3;
        mel_motif = forms[phrase & 7][mel_form];
        mel_relchord = RND() < 110;
    }
}

/* ----------------------------------------------------------------- pulse */
static uint8_t chord_note(uint8_t k, uint8_t base) {
    int8_t d = (int8_t)chord_deg + (int8_t)(k * 2);
    return clamp_note((int16_t)base + root + deg_semi(d));
}

static uint8_t euclid(uint8_t k, uint8_t s) {
    /* is step s (0..15) a hit in an even distribution of k hits? */
    return (uint8_t)(((uint16_t)(s * k) & 15u) < k);
}

static void pulse_step(void) {
    uint8_t style = (uint8_t)((mparam[P_PSTYLE] + style_swap) & 3);
    uint8_t mix = mparam[P_PULSE], vol, note;
    if (prof[district].style != 0xFF && mparam[P_INFLUENCE] > 7) style = prof[district].style;
    if (!mix || breath) return;
    switch (style) {
    case 0:   /* slow arpeggio of chord tones */
        if ((step & 1) || RND() > (uint8_t)(e_dens * 14 + 20)) return;
        note = chord_note(pu_k, 60);
        if (pu_dir) { if (pu_k == 0) pu_dir = 0; else --pu_k; }
        else { if (++pu_k >= 3) { pu_k = 2; pu_dir = 1; } }
        vol = mixvol((uint8_t)(7 + rn(4)), mix);
        if (vol && claim(1, 2, (uint8_t)(step_frames * 2))) {
            pu_pan = pu_pan == PAN_L ? PAN_R : PAN_L;
            snd_pulse(1, note, (uint8_t)((vol << 4) | 3), 2, pu_pan, 0);
        }
        break;
    case 1:   /* mechanism: euclidean pulses on the chord root */
        if (!euclid((uint8_t)(3 + (e_dens >> 2)), step)) return;
        note = chord_note(0, 48);
        vol = mixvol((uint8_t)(step == 0 ? 9 : 6), mix);
        if (vol && claim(1, 2, step_frames)) snd_pulse(1, note, (uint8_t)((vol << 4) | 1), 1, PAN_C, 0);
        break;
    case 2:   /* shimmer: a pseudo-chord, three tones alternating every frame pair */
        if ((step & 7) || RND() > (uint8_t)(e_dens * 12 + 60)) return;
        vol = mixvol((uint8_t)(6 + rn(3)), mix);
        if (vol && claim(1, 2, (uint8_t)(step_frames * 6))) {
            uint8_t k;
            for (k = 0; k < 3; ++k) sh_per[k] = snd_period(chord_note(k, 72));
            snd_pulse(1, chord_note(0, 72), (uint8_t)((vol << 4) | 5), 2, (RND() & 1) ? PAN_L : PAN_R, 0);
            sh_on = 1; sh_t = 0; sh_i = 0;
        }
        break;
    default:  /* distant intervals */
        if ((step % 6u) != 0 || RND() > (uint8_t)(e_dens * 12 + 50)) return;
        pu_k ^= 1;
        note = pu_k ? chord_note(0, 60) : chord_note(2, 60);
        vol = mixvol((uint8_t)(6 + rn(3)), mix);
        if (vol && claim(1, 2, (uint8_t)(step_frames * 3))) {
            snd_pulse(1, note, (uint8_t)((vol << 4) | 4), 2, pu_k ? PAN_L : PAN_R, 0);
            echo_push((uint8_t)(step_frames * 4), note, (uint8_t)(((vol >> 1) << 4) | 4), pu_k ? PAN_R : PAN_L);
        }
        break;
    }
}

static void shimmer_frame(void) {
    if (!sh_on) return;
    if (busy[1] == 0 || prio[1] != 2) { sh_on = 0; return; }
    if (++sh_t & 1) return;
    if (++sh_i >= 3) sh_i = 0;
    snd_pulse_period(1, sh_per[sh_i]);
}

/* ----------------------------------------------------------------- noise */
static uint8_t noise_mix(void) {
    uint8_t m = mparam[P_NOISE];
    if (lm_type == 5 && lm_gain) {           /* a quiet garden hushes the city */
        uint8_t cut = lm_gain > m ? m : lm_gain;
        m -= cut;
    }
    return m;
}

static void noise_frame(void) {
    uint8_t m = noise_mix(), p;
    if (!m || !music_rain) return;
    p = (uint8_t)((music_rain * m) >> 3);   /* rain: soft ticks at random */
    if (RND() < p && claim(3, 0, 2)) {
        static const uint8_t polys[4] = {0x01, 0x12, 0x21, 0x13};
        uint8_t v = (uint8_t)(1 + (RND() < (uint8_t)(music_rain * 8) ? 1 : 0));
        snd_noise(polys[RND() & 3], (uint8_t)((v << 4) | 1), (uint8_t)(RND() % 3u));
    }
}

static void noise_step(void) {
    uint8_t m = noise_mix(), v;
    if (!m) return;
    /* machinery in the industrial quarter (or the mechanism style) */
    if ((district == 1 || mparam[P_PSTYLE] == 1) && euclid((uint8_t)(2 + (e_noise >> 2)), (uint8_t)((step + 3) & 15))) {
        v = mixvol((uint8_t)(3 + (e_noise >> 2)), m);
        if (v && claim(3, 1, 4)) snd_noise((uint8_t)(0x0Bu | (RND() & 0x10u)), (uint8_t)((v << 4) | 1), PAN_C);
    }
    /* heavy rain: washes */
    if (music_rain > 10 && step == 8 && RND() < 120) {
        v = mixvol(3, m);
        if (v && claim(3, 1, 30)) snd_noise(0x31, (uint8_t)((v << 4) | 4), (RND() & 1) ? PAN_L : PAN_R);
    }
    /* wind: long soft breaths of noise */
    if (step == 0 && RND() < (uint8_t)(10 + e_noise * 6 + music_fog * 3)) {
        v = mixvol((uint8_t)(2 + (e_noise >> 3)), m);
        if (v && claim(3, 1, 90)) snd_noise((uint8_t)(0x52 + (RND() & 0x11u)), (uint8_t)((v << 4) | 7), (RND() & 1) ? PAN_L : PAN_R);
    }
}

static void thunder(void) {
    if (claim(3, 4, 160)) snd_noise(0x77, 0xA7, PAN_C);
    if (claim(0, 4, 90)) snd_pulse(0, (uint8_t)(36 + root), 0x77, 2, PAN_C, 0x7F);
}

/* ------------------------------------------------------------- landmarks */
static void landmark_step(void) {
    uint8_t g = lm_gain, v;
    if (!g || !lm_type) return;
    switch (lm_type) {
    case 1:   /* neon: electrical buzz in short clusters */
        if ((step & 3) == 0 && RND() < (uint8_t)(g * 10)) {
            v = (uint8_t)(1 + (g >> 3));
            if (claim(3, 3, 3)) snd_noise(0x0A, (uint8_t)((v << 4) | 1), PAN_C);
        }
        break;
    case 2:   /* substation: regular low-frequency pulse */
        if ((step & 7) == 0 && claim(3, 3, 8)) snd_noise(0x6E, (uint8_t)(((2 + (g >> 1)) << 4) | 2), PAN_C);
        if (step == 0 && claim(1, 3, 16)) snd_pulse(1, (uint8_t)(36 + root), (uint8_t)(((1 + (g >> 1)) << 4) | 3), 2, PAN_C, 0);
        break;
    case 3:   /* rooftop vents: a slow mechanical cycle */
        if (((bar & 1) == 0) && (step == 0 || step == 6) && claim(3, 3, 60))
            snd_noise(0x46, (uint8_t)(((1 + (g >> 2)) << 4) | 6), step ? PAN_R : PAN_L);
        break;
    case 4:   /* abandoned platform: distant repeated interval, faint hum */
        if ((step % 12u) == 0 && claim(1, 3, (uint8_t)(step_frames * 4))) {
            uint8_t n = (uint8_t)(72 + root + ((bar & 1) ? 7 : 3));
            snd_pulse(1, n, (uint8_t)(((1 + g / 3u) << 4) | 4), 2, (bar & 1) ? PAN_L : PAN_R, 0);
            echo_push((uint8_t)(step_frames * 5), n, (uint8_t)(((g / 6u + 1) << 4) | 4), (bar & 1) ? PAN_R : PAN_L);
        }
        if (step == 4 && bar == 0 && claim(3, 3, 120)) snd_noise(0x0F, 0x17, PAN_C);
        break;
    case 5:   /* quiet garden: delicate high bells */
        if ((step % 6u) == 2 && RND() < (uint8_t)(g * 10) && claim(0, 3, 12)) {
            static const uint8_t penta[5] = {0, 3, 5, 7, 10};
            uint8_t n = (uint8_t)(84 + root + penta[rn(5)]);
            snd_pulse(0, clamp_note(n), (uint8_t)(((2 + g / 4u) << 4) | 2), 2, (RND() & 1) ? PAN_L : PAN_R, 0);
        }
        break;
    }
}

/* ------------------------------------------------------- phrase / macro */
static void glide(uint8_t *v, uint8_t target) {
    if (*v < target) ++*v;
    else if (*v > target) --*v;
}

static void update_targets(void) {
    const Profile *p = &prof[district];
    uint8_t inf = mparam[P_INFLUENCE];
    int8_t t;
    /* district contributions scale with LOCATION INFLUENCE (0..15) */
#define INF(x) ((int8_t)(((int16_t)(x) * inf) >> 4))
    t = (int8_t)mparam[P_DENSITY] + INF(p->dens) - (music_rain == 0 && music_fog == 0 ? 1 : 0);
    if (breath) t -= 4;
    glide(&e_dens, clamp15(t));
    t = (int8_t)(mparam[P_NOISE] >> 1) + INF(p->noise) + (int8_t)(music_rain >> 1) + (music_storm ? 3 : 0);
    glide(&e_noise, clamp15(t));
    t = (int8_t)mparam[P_BRIGHT] - (int8_t)(music_fog >> 2) + (music_rain > 8 ? -1 : 0);
    glide(&e_bright, clamp15(t));
    t = (int8_t)mparam[P_TENSION] + INF(p->tens) + (music_storm ? 2 : 0);
    glide(&e_tens, clamp15(t));
    t = (int8_t)((p->melp * (int8_t)inf + 8 * (15 - (int8_t)inf)) >> 4);
    glide(&e_melp, clamp15(t));
    t = (int8_t)(((p->env * (int8_t)inf) >> 4) + (mparam[P_STABILITY] > 10 ? 1 : 0));
    glide(&e_env, (uint8_t)(t < 0 ? 0 : t > 4 ? 4 : t));
#undef INF
}

static void on_phrase(void) {
    uint8_t mut = mparam[P_MUTATION];
    ++phrase;
    /* register and tonal centre glide toward the place (one move per phrase) */
    {
        uint8_t inf = mparam[P_INFLUENCE];
        int8_t treg = inf > 7 ? prof[district].reg : 0;
        int8_t troot = inf > 5 ? prof[district].root : 0;
        if (e_reg < treg) e_reg += 12; else if (e_reg > treg) e_reg -= 12;
        if (!music_freeze && e_root_off != troot && RND() < (uint8_t)(inf * 14)) e_root_off = troot;
    }
    /* meso: mutation of the motif memory */
    if (RND() < (uint8_t)(mut * 14)) mutate_motif(rn(NMOT));
    if (RND() < (uint8_t)(mut * 6)) mutate_motif(mel_motif);
    mel_pan = (uint8_t)(RND() % 3u);
    /* breaths: whole phrases of near silence for the melody and pulse */
    breath = (uint8_t)(RND() < (uint8_t)(8 + (15 - e_dens) * 2));
    music_section = breath;
    /* macro evolution */
    if (macro_left) { --macro_left; }
    else {
        uint8_t k = RND();
        macro_left = (uint8_t)(2 + (mparam[P_STABILITY] >> 1) + rn(3));
        if (k < 50) {
            make_motif(rn(NMOT));
        } else if (k < 90) {
            if (!music_freeze && mparam[P_STABILITY] < 13)
                cur_mode = (cur_mode == mparam[P_MODE]) ? mode_drift[mparam[P_MODE]] : mparam[P_MODE];
        } else if (k < 130) {
            reg_drift = (int8_t)((RND() < 128) ? 0 : (RND() & 1) ? 12 : -12);
        } else if (k < 165) {
            wave_off = (uint8_t)(wave_off ? 0 : 1 + rn(2));
        } else if (k < 195) {
            style_swap = (uint8_t)(style_swap ? 0 : 2);
        } else if (!music_freeze) {
            /* drift the tonal centre a fifth up or down, then home again */
            if (fifths != 0) fifths = 0;
            else fifths = (RND() & 1) ? 1 : -1;
        }
    }
    if (cur_mode >= MODE_COUNT) cur_mode = 0;
    music_cur_mode = cur_mode;
    set_root();
    drone_retune(0);
}

static void on_bar(void) {
    update_targets();
    if (bar == 0) on_phrase();
    if (!music_freeze && --chord_left == 0) {
        chord_left = mparam[P_HARM] ? mparam[P_HARM] : 4;
        next_chord();
        drone_retune(0);
    } else if (music_freeze && chord_left == 0) {
        chord_left = 1;
    }
    /* drone breathing: level swells on alternate bars when MOTION is high */
    if (mparam[P_MOTION] >= 2 && drone_level > 1) snd_wave_level((bar & 1) ? drone_level - 1 : drone_level);
    else if (drone_level != drone_lvl_target()) drone_retune(0);
}

static void on_step(void) {
    ++music_steps;
    if (step == 0) on_bar();
    melody_step();
    pulse_step();
    noise_step();
    landmark_step();
    if (music_storm && step == 5 && bar == 2 && RND() < 40) thunder();
    if (++step >= 16) { step = 0; if (++bar >= 4) bar = 0; }
}

/* ------------------------------------------------------------- interface */
static uint8_t music_on;

void music_tick(void) {
    uint8_t i;
    if (!music_on) return;
    for (i = 0; i < 4; ++i) if (busy[i]) --busy[i];
    echo_frame();
    drone_frame();
    melody_frame();
    shimmer_frame();
    noise_frame();
    if (++frames >= step_frames) {
        frames = 0;
        step_frames = (uint8_t)(20 - mparam[P_SPEED]);
        on_step();
    }
}

void music_reseed(uint16_t seed) {
    uint8_t i;
    music_seed = seed;
    mrs = seed ? seed : 0x1234u;
    for (i = 0; i < NMOT; ++i) make_motif(i);
    mel_motif = 0; mel_i = 0; mel_wait = 2; mel_form = 0;
}

void music_init(uint16_t seed) {
    uint8_t i;
    music_on = 0;
    snd_init();
    for (i = 0; i < 4; ++i) { busy[i] = 0; prio[i] = 0; echo[i].t = 0; }
    music_reseed(seed);
    cur_mode = mparam[P_MODE] & 7;
    e_dens = mparam[P_DENSITY]; e_noise = mparam[P_NOISE] >> 1; e_bright = mparam[P_BRIGHT];
    e_tens = mparam[P_TENSION]; e_melp = 8; e_env = 2; e_reg = 0; e_root_off = 0;
    fifths = 0; reg_drift = 0; wave_off = 0; style_swap = 0; breath = 0;
    chord_deg = 0; chord_left = mparam[P_HARM] ? mparam[P_HARM] : 4;
    macro_left = 3;
    step = 0; bar = 1; phrase = 0; frames = 0;
    step_frames = (uint8_t)(20 - mparam[P_SPEED]);
    set_root();
    music_cur_mode = cur_mode;
    drone_retune(1);
    music_on = 1;
}

void music_set_location(uint8_t d, uint8_t lm, uint8_t lm_dist) {
    uint8_t g;
    district = d < 5 ? d : 0;
    lm_type = lm;
    g = lm_dist >= 15 ? 0 : (uint8_t)(15 - lm_dist);
    if (music_focus) g = g > 7 ? 15 : (uint8_t)(g << 1);
    lm_gain = g;
}

void music_event(uint8_t ev) {
    switch (ev) {
    case EV_TRAIN:
        if (claim(3, 4, 150)) snd_noise(0x64, 0x87, PAN_C);
        if (claim(0, 4, 100)) snd_pulse(0, (uint8_t)(79 + root), 0x47, 0, PAN_R, 0x1F);
        break;
    case EV_BUZZ:
        if (claim(3, 2, 4)) snd_noise(0x09, 0x21, PAN_C);
        break;
    case EV_THUNDER:
        thunder();
        break;
    }
}

void music_load_preset(uint8_t n) {
    uint8_t i;
    n %= PRESET_COUNT;
    for (i = 0; i < NPARAM; ++i)
        if (i != P_ATMOS && i != P_INFLUENCE) mparam[i] = presets[n][i];
    cur_mode = mparam[P_MODE];
    music_reseed(preset_seed[n]);
    set_root();
    drone_retune(1);
}

/* A random but coherent configuration: values are drawn inside ranges that
 * keep the result listenable, and some choices constrain others. */
void music_randomise(void) {
    uint8_t m = rn(MODE_COUNT);
    mparam[P_MODE] = m;
    mparam[P_ROOT] = rn(12);
    mparam[P_DRONE] = (uint8_t)(7 + rn(7));
    mparam[P_MELODY] = (uint8_t)(6 + rn(7));
    mparam[P_PULSE] = (uint8_t)(4 + rn(8));
    mparam[P_NOISE] = (uint8_t)(2 + rn(8));
    mparam[P_SPEED] = (uint8_t)(2 + rn(9));
    mparam[P_DENSITY] = (uint8_t)(2 + rn(8 + (mparam[P_SPEED] < 6 ? 3 : 0)));
    mparam[P_TENSION] = (uint8_t)(m == 2 || m == 7 ? 6 + rn(7) : 1 + rn(8));
    mparam[P_BRIGHT] = (uint8_t)(m == 6 || m == 5 ? 8 + rn(7) : 2 + rn(9));
    mparam[P_MUTATION] = (uint8_t)(2 + rn(9));
    mparam[P_STABILITY] = (uint8_t)(3 + rn(11));
    mparam[P_PSTYLE] = rn(4);
    mparam[P_WAVE] = rn(5);
    mparam[P_HARM] = (uint8_t)(1u << rn(4));
    mparam[P_ECHO] = rn(3);
    mparam[P_MOTION] = rn(4);
    if (mparam[P_PSTYLE] == 1 && mparam[P_SPEED] < 5) mparam[P_SPEED] = 5;
    cur_mode = m;
    music_reseed((uint16_t)(RND() << 8) | RND());
    set_root();
    drone_retune(1);
}
