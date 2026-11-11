/* VACANT - generative composer.
 *
 * music_tick() runs from the VBlank interrupt (~59.7 Hz) whatever the main
 * loop is doing, so the music never depends on walking speed or frame drops.
 * Musical roles (DRONE, MELODY, PULSE, NOISE) are mapped onto the four real
 * channels; echoes, landmarks and city events borrow channels by priority.
 */
#ifndef VACANT_MUSIC_H
#define VACANT_MUSIC_H
#include "../core/platform.h"

/* user parameters (synthesiser pages); ranges in music.c param_max[] */
#define P_DRONE 0u
#define P_MELODY 1u
#define P_PULSE 2u
#define P_NOISE 3u
#define P_ROOT 4u
#define P_MODE 5u
#define P_BRIGHT 6u
#define P_TENSION 7u
#define P_DENSITY 8u
#define P_MUTATION 9u
#define P_SPEED 10u
#define P_STABILITY 11u
#define P_ATMOS 12u
#define P_INFLUENCE 13u
/* preset-only character parameters */
#define P_PSTYLE 14u     /* pulse style: 0 arpeggio 1 mechanism 2 shimmer 3 intervals */
#define P_WAVE 15u       /* drone timbre 0..4 */
#define P_HARM 16u       /* bars per chord 1..8 */
#define P_ECHO 17u       /* echo repeats 0..2 */
#define P_MOTION 18u     /* vibrato and drift depth 0..3 */
#define NPARAM 19u

#define MODE_COUNT 8u
#define PRESET_COUNT 5u

/* city events */
#define EV_TRAIN 1u
#define EV_BUZZ 2u
#define EV_THUNDER 3u

extern uint8_t mparam[NPARAM];
extern const uint8_t param_max[NPARAM];
extern uint16_t music_seed;
extern uint8_t music_freeze;        /* FREEZE HARMONY */
extern uint8_t music_focus;         /* ambient-focus mode (A button) */
extern uint8_t music_rain, music_fog, music_storm;   /* 0..16, 0..16, 0/1 */
extern volatile uint16_t music_steps;     /* diagnostics */
extern volatile uint8_t music_cur_root, music_cur_mode, music_cur_degree;
extern volatile uint8_t music_section;    /* 0 normal, 1 breath (sparse) */

void music_init(uint16_t seed);
void music_tick(void);            /* VBlank interrupt */
void music_set_location(uint8_t district, uint8_t landmark, uint8_t lm_dist);
void music_event(uint8_t ev);
void music_load_preset(uint8_t n); /* 0..PRESET_COUNT-1 */
void music_randomise(void);
void music_reseed(uint16_t seed); /* new motifs, keeps the sounding drone */
const char *music_mode_name(uint8_t m);
const char *music_preset_name(uint8_t n);
uint8_t music_rand(void);
#endif
