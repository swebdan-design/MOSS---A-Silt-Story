/* VACANT - sound hardware layer. Home bank: everything here, including the
 * period and wave tables, must stay mapped while the VBlank interrupt runs. */
#include <gb/gb.h>
#include <gb/hardware.h>
#include "sound.h"
#include "audio_tables.inc"

uint8_t snd_wave_loaded = 0xFF;
uint16_t snd_triggers;
static uint8_t pan_bits = 0xFF;
static const uint8_t wave_lvl[4] = {0x00, 0x60, 0x40, 0x20};

void snd_init(void) {
    NR52_REG = 0x80;
    NR50_REG = 0x77;
    NR51_REG = 0xFF;
    pan_bits = 0xFF;
    NR10_REG = 0x08;
    NR12_REG = 0; NR22_REG = 0; NR42_REG = 0;
    NR30_REG = 0;
    snd_wave_loaded = 0xFF;
}

void snd_mute_all(void) {
    NR12_REG = 0; NR14_REG = 0x80;
    NR22_REG = 0; NR24_REG = 0x80;
    NR30_REG = 0;
    NR42_REG = 0; NR44_REG = 0x80;
}

uint16_t snd_period(uint8_t note) {
    if (note < NOTE_LO) note = NOTE_LO;
    if (note > NOTE_HI) note = NOTE_HI;
    return note_period[note - NOTE_LO];
}

void snd_pan(uint8_t ch, uint8_t pan) {
    uint8_t r = (uint8_t)(1u << ch), l = (uint8_t)(0x10u << ch);
    uint8_t b = pan_bits | l | r;
    if (pan == PAN_L) b &= (uint8_t)~r;
    else if (pan == PAN_R) b &= (uint8_t)~l;
    if (b != pan_bits) { pan_bits = b; NR51_REG = b; }
}

void snd_pulse(uint8_t ch, uint8_t note, uint8_t env, uint8_t duty, uint8_t pan, uint8_t sweep) {
    uint16_t p = snd_period(note);
    snd_pan(ch, pan);
    ++snd_triggers;
    if (ch == 0) {
        NR10_REG = sweep ? sweep : 0x08;
        NR11_REG = (uint8_t)(duty << 6);
        NR12_REG = env;
        NR13_REG = (uint8_t)p;
        NR14_REG = 0x80 | ((uint8_t)(p >> 8) & 7);
    } else {
        NR21_REG = (uint8_t)(duty << 6);
        NR22_REG = env;
        NR23_REG = (uint8_t)p;
        NR24_REG = 0x80 | ((uint8_t)(p >> 8) & 7);
    }
}

void snd_pulse_period(uint8_t ch, uint16_t p) {
    if (ch == 0) { NR13_REG = (uint8_t)p; NR14_REG = (uint8_t)(p >> 8) & 7; }
    else { NR23_REG = (uint8_t)p; NR24_REG = (uint8_t)(p >> 8) & 7; }
}

/* Load a timbre with the DAC off (wave RAM is only safe to write then),
 * and restart the channel. Retuning a running drone uses snd_wave_period. */
void snd_wave_start(uint8_t note, uint8_t wave, uint8_t level, uint8_t pan) {
    uint8_t i;
    uint16_t p = snd_period((uint8_t)(note + 12));   /* wave plays an octave low */
    snd_pan(2, pan);
    ++snd_triggers;
    NR30_REG = 0x00;
    if (wave != snd_wave_loaded && wave < WAVE_COUNT) {
        for (i = 0; i < 16; ++i) AUD3WAVE[i] = wave_table[wave][i];
        snd_wave_loaded = wave;
    }
    NR30_REG = 0x80;
    NR31_REG = 0x00;
    NR32_REG = wave_lvl[level & 3];
    NR33_REG = (uint8_t)p;
    NR34_REG = 0x80 | ((uint8_t)(p >> 8) & 7);
}

void snd_wave_period(uint16_t p) {
    NR33_REG = (uint8_t)p;
    NR34_REG = (uint8_t)(p >> 8) & 7;
}

void snd_wave_level(uint8_t level) {
    NR32_REG = wave_lvl[level & 3];
}

void snd_noise(uint8_t poly, uint8_t env, uint8_t pan) {
    snd_pan(3, pan);
    ++snd_triggers;
    NR41_REG = 0x00;
    NR42_REG = env;
    NR43_REG = poly;
    NR44_REG = 0x80;
}
