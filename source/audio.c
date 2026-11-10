/* MOSS — coordinated four-channel sound scheduler. Home bank (bank 0):
 * the handler and every table it reads must stay mapped at all times. */
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "audio.h"
#include "audio_tables.inc"

#define QUEUE_LEN 24
#define FREE 0xFF

typedef struct {
    uint16_t at;
    SndEv e;
} QEv;

typedef struct {
    uint8_t prio, hold, fx, fxarg;
    uint16_t per, base, target;
    uint8_t vib;
    uint8_t trill_left, trill_timer, env, duty;
    uint8_t stage, stage_timer;
} Chan;

static QEv queue[QUEUE_LEN];
static Chan chan[4];
volatile uint16_t audio_tick;
uint8_t audio_dropped;
uint16_t audio_played;
uint8_t audio_stereo = 1;
volatile uint8_t audio_last_note[4];

static uint8_t pan_bits = 0xFF;
static uint8_t wave_loaded = 0xFF;
static uint8_t drone_on, drone_level;
static uint16_t drone_per;
static const int8_t vib_shape[8] = {0, 1, 2, 1, 0, -1, -2, -1};
static const uint8_t wlevel_code[4] = {0x00, 0x60, 0x40, 0x20}; /* off,25,50,100 */

static uint16_t period_of(uint8_t note) {
    if (note < NOTE_LO) note = NOTE_LO;
    if (note > NOTE_HI) note = NOTE_HI;
    return note_period[note - NOTE_LO];
}

static void set_period(uint8_t ch, uint16_t p) {
    if (ch == 0) { NR13_REG = (uint8_t)p; NR14_REG = (uint8_t)(p >> 8) & 7; }
    else if (ch == 1) { NR23_REG = (uint8_t)p; NR24_REG = (uint8_t)(p >> 8) & 7; }
    else if (ch == 2) { NR33_REG = (uint8_t)p; NR34_REG = (uint8_t)(p >> 8) & 7; }
}

static void set_pan(uint8_t ch, uint8_t pan) {
    uint8_t right = (uint8_t)(1u << ch), left = (uint8_t)(0x10u << ch);
    uint8_t bits = pan_bits | left | right;
    if (audio_stereo) {
        if (pan == PAN_L) bits &= (uint8_t)~right;
        else if (pan == PAN_R) bits &= (uint8_t)~left;
    }
    if (bits != pan_bits) { pan_bits = bits; NR51_REG = bits; }
}

/* Start the wave channel safely on DMG: DAC off before touching wave RAM,
 * so a retrigger can never corrupt the pattern being read. */
static void wave_start(uint8_t wave, uint16_t per, uint8_t level) {
    uint8_t i;
    NR30_REG = 0x00;
    if (wave != wave_loaded && wave < WAVE_COUNT) {
        for (i = 0; i < 16; ++i) AUD3WAVE[i] = wave_table[wave][i];
        wave_loaded = wave;
    }
    NR30_REG = 0x80;
    NR31_REG = 0x00;
    NR32_REG = wlevel_code[level & 3];
    NR33_REG = (uint8_t)per;
    NR34_REG = 0x80 | ((uint8_t)(per >> 8) & 7);
}

static void trigger_pulse_noise(uint8_t ch, Chan *c) {
    uint16_t p = c->per;
    if (ch == 0) {
        NR10_REG = (c->fx & FX_SWEEP) ? c->fxarg : 0x08;
        NR11_REG = (uint8_t)(c->duty << 6);
        NR12_REG = c->env;
        NR13_REG = (uint8_t)p;
        NR14_REG = 0x80 | ((uint8_t)(p >> 8) & 7);
    } else if (ch == 1) {
        NR21_REG = (uint8_t)(c->duty << 6);
        NR22_REG = c->env;
        NR23_REG = (uint8_t)p;
        NR24_REG = 0x80 | ((uint8_t)(p >> 8) & 7);
    } else {
        NR41_REG = 0x00;
        NR42_REG = c->env;
        NR43_REG = c->duty; /* noise: polynomial setting */
        NR44_REG = 0x80;
    }
}

static void start_event(const SndEv *e) {
    uint8_t ch = e->ch & 3;
    Chan *c = &chan[ch];
    if (c->hold && e->prio < c->prio) { ++audio_dropped; return; }
    c->prio = e->prio;
    c->hold = e->hold ? e->hold : 1;
    c->fx = e->fx;
    c->fxarg = e->fxarg;
    c->vib = 0;
    c->trill_left = 0;
    audio_last_note[ch] = e->note;
    ++audio_played;
    set_pan(ch, e->pan);
    if (ch == 3) {
        c->env = e->env;
        c->duty = e->note;
        if (e->fx & FX_TRILL) { c->trill_left = e->fxarg >> 4; c->trill_timer = e->fxarg & 15; }
        trigger_pulse_noise(3, c);
        return;
    }
    c->per = c->base = period_of(ch == 2 ? (uint8_t)(e->note + 12) : e->note);
    if (ch == 2) {
        if (e->fx & FX_DRONE) {
            uint8_t was_on = drone_on;
            drone_on = 1;
            drone_level = e->env & 3;
            if (was_on && wave_loaded == e->timbre) {
                /* Glide from the sounding pitch: no retrigger, no click. */
                c->target = c->base;
                c->per = drone_per;
                c->fxarg = e->fxarg ? e->fxarg : 1;
                NR32_REG = wlevel_code[drone_level];
            } else {
                drone_per = c->per;
                c->target = c->per;
                wave_start(e->timbre, c->per, drone_level);
            }
            c->hold = 0; /* a drone never blocks other events */
            c->prio = 0;
            return;
        }
        c->stage = 0;
        c->stage_timer = e->hold >> 2;
        if (drone_on && wave_loaded == e->timbre) {
            /* Accent the sounding water voice: jump pitch, raise level. */
            set_period(2, c->per);
            NR32_REG = wlevel_code[3];
            drone_per = c->per;
        } else {
            wave_start(e->timbre, c->per, 3);
        }
        return;
    }
    c->env = e->env;
    c->duty = e->timbre & 3;
    if (e->fx & FX_TRILL) { c->trill_left = e->fxarg >> 4; c->trill_timer = e->fxarg & 15; }
    trigger_pulse_noise(ch, c);
}

static void chan_frame(uint8_t ch) {
    Chan *c = &chan[ch];
    if (ch == 2 && drone_on && (c->fx & FX_DRONE)) {
        /* Portamento toward the drone target (slow harmonic movement). */
        if (c->per != c->target) {
            uint8_t rate = c->fxarg;
            if (c->per < c->target) { c->per += rate; if (c->per > c->target) c->per = c->target; }
            else { if (c->per - c->target < rate) c->per = c->target; else c->per -= rate; }
            drone_per = c->per;
            set_period(2, c->per);
        }
        return;
    }
    if (!c->hold) return;
    --c->hold;
    if (c->fx & FX_SLIDE) {
        c->per += (int8_t)c->fxarg;
        if (c->per > 2040) c->per = 2040;
        set_period(ch, c->per);
    }
    if (c->fx & FX_VIB) {
        int16_t d = vib_shape[(c->vib >> 2) & 7] * (int8_t)c->fxarg;
        ++c->vib;
        set_period(ch, (uint16_t)((int16_t)c->base + d));
    }
    if (c->trill_left && --c->trill_timer == 0) {
        --c->trill_left;
        c->trill_timer = c->fxarg & 15;
        if (c->env >= 0x20) c->env -= 0x20;
        trigger_pulse_noise(ch, c);
    }
    if (ch == 2) {
        if (c->stage_timer && --c->stage_timer == 0) {
            ++c->stage;
            c->stage_timer = (c->fx & FX_WDECAY) ? 6 : 10;
            if (drone_on) {
                NR32_REG = wlevel_code[c->stage >= 2 ? drone_level : 2];
            } else {
                NR32_REG = wlevel_code[c->stage >= 3 ? 0 : 3 - c->stage];
            }
        }
        if (!c->hold && !drone_on) NR32_REG = 0;
        if (!c->hold && drone_on) {
            NR32_REG = wlevel_code[drone_level];
            c->fx = FX_DRONE; c->target = c->per; c->fxarg = 1;
        }
    }
}

void audio_isr(void) {
    uint8_t i;
    uint16_t now = ++audio_tick;
    for (i = 0; i < QUEUE_LEN; ++i) {
        if (queue[i].e.ch != FREE && (int16_t)(now - queue[i].at) >= 0) {
            start_event(&queue[i].e);
            queue[i].e.ch = FREE;
        }
    }
    chan_frame(0);
    chan_frame(1);
    chan_frame(2);
    chan_frame(3);
}

uint8_t snd_play(const SndEv *e, uint8_t delay) {
    uint8_t i, victim = FREE, low = 0xFF;
    uint16_t at = audio_tick + (delay ? delay : 1);
    for (i = 0; i < QUEUE_LEN; ++i) {
        if (queue[i].e.ch == FREE) { victim = i; break; }
        if (queue[i].e.prio < low) { low = queue[i].e.prio; victim = i; }
    }
    if (queue[victim].e.ch != FREE && low >= e->prio) { ++audio_dropped; return 0; }
    /* The handler reads queue entries; mark busy until fully written. */
    queue[victim].e.ch = FREE;
    queue[victim].at = at;
    queue[victim].e.note = e->note;
    queue[victim].e.env = e->env;
    queue[victim].e.timbre = e->timbre;
    queue[victim].e.hold = e->hold;
    queue[victim].e.prio = e->prio;
    queue[victim].e.fx = e->fx;
    queue[victim].e.fxarg = e->fxarg;
    queue[victim].e.pan = e->pan;
    queue[victim].e.ch = e->ch & 3;
    return 1;
}

uint8_t snd_queue_used(void) {
    uint8_t i, n = 0;
    for (i = 0; i < QUEUE_LEN; ++i) n += queue[i].e.ch != FREE;
    return n;
}

void snd_drone_off(void) {
    __critical {
        drone_on = 0;
        chan[2].fx = 0;
        chan[2].hold = 0;
        NR32_REG = 0;
    }
}

void snd_flush(void) {
    uint8_t i;
    __critical {
        for (i = 0; i < QUEUE_LEN; ++i) queue[i].e.ch = FREE;
        for (i = 0; i < 4; ++i) { chan[i].hold = 0; chan[i].trill_left = 0; chan[i].fx = 0; }
        drone_on = 0;
        /* Release by envelope: volume 0, decreasing. No DAC switch for pulses. */
        NR12_REG = 0x08; NR22_REG = 0x08; NR42_REG = 0x08;
        NR32_REG = 0;
    }
}

void snd_master(uint8_t level) {
    level &= 7;
    NR50_REG = (uint8_t)((level << 4) | level);
}

void audio_init(void) {
    uint8_t i;
    NR52_REG = 0x80;
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
    pan_bits = 0xFF;
    NR10_REG = 0x08;
    for (i = 0; i < QUEUE_LEN; ++i) queue[i].e.ch = FREE;
    memset(chan, 0, sizeof(chan));
    wave_loaded = 0xFF;
    drone_on = 0;
    NR30_REG = 0;
}

/* --- small UI sounds: short, quiet, on the living channel --- */
void sfx_blip(void) {
    static const SndEv e = {CH_LIVING, 84, 0x51, 2, 4, PRIO_UI, 0, 0, PAN_C};
    snd_play(&e, 0);
}
void sfx_confirm(void) {
    static const SndEv a = {CH_LIVING, 81, 0x61, 2, 5, PRIO_UI, 0, 0, PAN_C};
    static const SndEv b = {CH_LIVING, 88, 0x52, 2, 8, PRIO_UI, 0, 0, PAN_C};
    snd_play(&a, 0);
    snd_play(&b, 5);
}
void sfx_page(void) {
    static const SndEv e = {CH_ENV, 0x21, 0x31, 0, 4, PRIO_UI, 0, 0, PAN_C};
    snd_play(&e, 0);
}
void sfx_step_stone(uint8_t note) {
    static SndEv e;
    e.ch = CH_PEBBLE; e.note = note; e.env = 0xA3; e.timbre = 2; e.hold = 20;
    e.prio = PRIO_PLAYER; e.fx = 0; e.fxarg = 0; e.pan = PAN_C;
    snd_play(&e, 0);
}
