/* Host stand-in for src/audio/sound.c: records every voice event so the
 * composer can be analysed without hardware. */
#include <stdio.h>
#include "../src/audio/sound.h"
#include "host_sound_stub.h"

uint8_t snd_wave_loaded = 0xFF;
uint16_t snd_triggers;
SndLog snd_log[LOG_MAX];
long snd_nlog;
long snd_now;      /* frame counter maintained by the test */
uint8_t snd_wave_level_now;

static void rec(uint8_t ch, uint8_t note, uint8_t vol, uint8_t extra) {
    if (snd_nlog < LOG_MAX) {
        SndLog *l = &snd_log[snd_nlog++];
        l->t = snd_now; l->ch = ch; l->note = note; l->vol = vol; l->extra = extra;
    }
    ++snd_triggers;
}

void snd_init(void) { snd_wave_loaded = 0xFF; }
void snd_mute_all(void) {}
uint16_t snd_period(uint8_t note) { return (uint16_t)(2048 - 131072.0 / (440.0 * __builtin_pow(2.0, (note - 69) / 12.0))); }
void snd_pan(uint8_t ch, uint8_t pan) { (void)ch; (void)pan; }
void snd_pulse(uint8_t ch, uint8_t note, uint8_t env, uint8_t duty, uint8_t pan, uint8_t sweep) {
    (void)duty; (void)pan; rec(ch, note, env >> 4, sweep);
}
void snd_pulse_period(uint8_t ch, uint16_t per) { (void)ch; (void)per; }
void snd_wave_start(uint8_t note, uint8_t wave, uint8_t level, uint8_t pan) {
    (void)pan; snd_wave_loaded = wave; snd_wave_level_now = level; rec(2, note, level, wave);
}
void snd_wave_period(uint16_t per) { (void)per; }
void snd_wave_level(uint8_t level) { snd_wave_level_now = level; }
void snd_noise(uint8_t poly, uint8_t env, uint8_t pan) { (void)pan; rec(3, poly, env >> 4, 0); }
