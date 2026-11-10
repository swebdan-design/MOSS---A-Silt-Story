/* MOSS — battery-backed save format (portable: Game Boy + host tests). */
#ifndef MOSS_SAVE_H
#define MOSS_SAVE_H
#include <stdint.h>
#include "compose.h"

#define SAVE_MAGIC 0x4D53   /* "MS" */
#define SAVE_VERSION 1
#define SAVE_SLOT_SIZE 0x400 /* two copies at SRAM 0xA000 and 0xA400 */

typedef struct {
    uint8_t text_speed, listen_time, stereo, volume;
} Settings;

typedef struct {
    uint16_t magic;
    uint8_t version, seq;
    uint8_t day, postgame, map, x, y, dir;
    uint8_t flags[8];
    uint8_t found, found_count;
    uint8_t found_order[8];
    uint16_t seed;
    uint16_t minutes;
    Settings settings;
    PondConfig pond;
    PondConfig my_pond;
    uint8_t my_pond_saved, preset;
    MusicMemory mmem;
    uint8_t pad[4];
    uint16_t checksum;
} SaveData;

extern SaveData game;
extern uint8_t save_status; /* see SAVE_* */
#define SAVE_NONE 0
#define SAVE_OK 1
#define SAVE_RECOVERED 2    /* newest copy bad, older copy used */
#define SAVE_CORRUPT 3      /* data present but no valid copy */

uint16_t save_checksum(const uint8_t *p, uint16_t n);
uint8_t save_validate(const SaveData *s);
uint8_t save_probe(void);
uint8_t save_load(void);
uint8_t save_write(void);
void save_erase(void);
/* platform: raw SRAM access */
void sram_read(uint16_t offset, void *dst, uint16_t n);
void sram_write(uint16_t offset, const void *src, uint16_t n);
#endif
