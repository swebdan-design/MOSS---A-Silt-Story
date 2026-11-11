/* VACANT - battery-backed saves (MBC5 SRAM, 8 KiB, two mirrored slots).
 *
 * Each slot holds a complete SaveData record with a sequence number and a
 * Fletcher-16 checksum. Writes alternate between slots, so a power cut
 * during a write can only damage the older copy; loading picks the newest
 * slot whose checksum verifies. The city itself is never stored: the seed
 * and coordinates rebuild it exactly. */
#ifndef VACANT_SAVE_H
#define VACANT_SAVE_H
#include "../core/platform.h"
#include "../music/music.h"

#define SAVE_MAGIC 0x5643u      /* "VC" */
#define SAVE_VERSION 1u
#define USER_PRESETS 4u
#define SLOT_A 0x0000u
#define SLOT_B 0x0100u

typedef struct {
    uint16_t magic;
    uint8_t version, seq;
    uint8_t has_place;          /* a position has been saved          */
    uint16_t world_seed;
    uint16_t px, py;
    uint8_t face;
    uint16_t music_seed;
    uint8_t params[NPARAM];     /* synthesiser settings               */
    uint8_t weather_mode;
    uint8_t freeze;
    uint8_t preset_used;        /* bit per user preset slot           */
    uint8_t presets[USER_PRESETS][NPARAM];
    uint16_t preset_seed[USER_PRESETS];
    uint16_t sum;
} SaveData;

extern SaveData save_ram;       /* working copy of what is on the cartridge */
extern uint8_t save_status;     /* result of the last load (see below)      */
#define SAVE_NONE 0u            /* blank or foreign SRAM                    */
#define SAVE_OK 1u
#define SAVE_RECOVERED 2u       /* newest slot was damaged; older one used  */

uint16_t save_checksum(const uint8_t *p, uint16_t n);
uint8_t save_load(void);        /* fills save_ram; returns save_status      */
uint8_t save_store(void);       /* writes save_ram to the next slot; 1 = verified */
void save_defaults(void);

/* SRAM access: the cartridge on hardware, an array in host tests */
extern uint8_t *save_sram;
void save_sram_on(void);
void save_sram_off(void);
#endif
