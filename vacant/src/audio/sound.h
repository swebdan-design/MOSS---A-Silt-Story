/* VACANT - Game Boy Color sound hardware layer (home bank, interrupt-safe).
 * Four real voices: CH1 pulse+sweep, CH2 pulse, CH3 wave, CH4 noise. */
#ifndef VACANT_SOUND_H
#define VACANT_SOUND_H
#include "../core/platform.h"

#define PAN_C 0u
#define PAN_L 1u
#define PAN_R 2u

void snd_init(void);
void snd_mute_all(void);
/* pulse: ch 0 or 1. env = NRx2 value (vol<<4 | dir<<3 | period). */
void snd_pulse(uint8_t ch, uint8_t note, uint8_t env, uint8_t duty, uint8_t pan, uint8_t sweep);
void snd_pulse_period(uint8_t ch, uint16_t per);   /* retune without retrigger */
uint16_t snd_period(uint8_t note);
/* wave: level 0..3 (off, 25, 50, 100%) */
void snd_wave_start(uint8_t note, uint8_t wave, uint8_t level, uint8_t pan);
void snd_wave_period(uint16_t per);
void snd_wave_level(uint8_t level);
/* noise: poly = NR43 value */
void snd_noise(uint8_t poly, uint8_t env, uint8_t pan);
void snd_pan(uint8_t ch, uint8_t pan);

extern uint8_t snd_wave_loaded;
extern uint16_t snd_triggers;    /* diagnostics: notes started */
#endif
