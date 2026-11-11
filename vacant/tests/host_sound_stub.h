#ifndef HOST_SOUND_STUB_H
#define HOST_SOUND_STUB_H
#include <stdint.h>
#define LOG_MAX 4000000L
typedef struct { long t; uint8_t ch, note, vol, extra; } SndLog;
extern SndLog snd_log[LOG_MAX];
extern long snd_nlog, snd_now;
extern uint8_t snd_wave_level_now;
#endif
