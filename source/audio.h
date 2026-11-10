/* MOSS — coordinated four-channel sound scheduler (home bank).
 *
 * One owner writes the APU registers: the VBlank handler audio_isr().
 * Everything else (the generative composer, UI sounds, story motifs) only
 * places small events in a timestamped queue with snd_play(). The handler
 * dispatches due events, arbitrates channel ownership by priority and runs
 * cheap per-frame effects (vibrato, glide, trill, wave-volume steps).
 */
#ifndef MOSS_AUDIO_H
#define MOSS_AUDIO_H
#include <stdint.h>

/* Conceptual voices -> DMG channels */
#define CH_PEBBLE 0 /* pulse 1 (with sweep) */
#define CH_LIVING 1 /* pulse 2 */
#define CH_WATER  2 /* programmable wave */
#define CH_ENV    3 /* noise */

/* Effect flags */
#define FX_NONE   0x00
#define FX_SWEEP  0x01 /* ch1: fxarg is NR10 */
#define FX_VIB    0x02 /* fxarg: depth (period units) */
#define FX_TRILL  0x04 /* fxarg: hi nibble repeats, lo nibble gap frames */
#define FX_SLIDE  0x08 /* fxarg: signed period delta per frame */
#define FX_DRONE  0x10 /* ch3: sustain at level, glide to new pitch */
#define FX_ACCENT 0x20 /* ch3: 100% -> 50% -> drone level over hold */
#define FX_WDECAY 0x40 /* ch3: 100% -> 50% -> 25% -> off over hold */
#define FX_KEEPWAVE 0x80

/* Priorities */
#define PRIO_AMBIENT 1
#define PRIO_ECHO    2
#define PRIO_MUSIC   3
#define PRIO_PLAYER  5
#define PRIO_MOTIF   6
#define PRIO_UI      7

/* Pan */
#define PAN_C 0
#define PAN_L 1
#define PAN_R 2

typedef struct {
    uint8_t ch;     /* CH_* */
    uint8_t note;   /* MIDI note; noise: NR43 value */
    uint8_t env;    /* NRx2 for pulse/noise; ch3: level 0..3 */
    uint8_t timbre; /* pulse: duty 0..3; ch3: wave id */
    uint8_t hold;   /* frames the channel is owned */
    uint8_t prio;
    uint8_t fx;
    uint8_t fxarg;
    uint8_t pan;
} SndEv;

#define WAVE_SOFT   0
#define WAVE_HOLLOW 1
#define WAVE_WARM   2
#define WAVE_REED   3
#define WAVE_COUNT  4

extern volatile uint16_t audio_tick;    /* frames since boot */
extern uint8_t audio_dropped;            /* diagnostics */
extern uint16_t audio_played;            /* diagnostics */
extern uint8_t audio_stereo;             /* 1 = use panning */
extern volatile uint8_t audio_last_note[4];

void audio_init(void);
void audio_isr(void);
/* Queue an event to start `delay` frames from now. Returns 0 if dropped. */
uint8_t snd_play(const SndEv *e, uint8_t delay);
/* Clear pending events and release all channels (with a soft fade). */
void snd_flush(void);
void snd_drone_off(void);
void snd_master(uint8_t level); /* 0..7, applied to both sides */
uint8_t snd_queue_used(void);

/* Small fixed UI sounds */
void sfx_blip(void);
void sfx_confirm(void);
void sfx_page(void);
void sfx_step_stone(uint8_t note);
#endif
