/* MOSS — save logic. Two copies, alternating, each with magic, version,
 * sequence number and a Fletcher-16 checksum. A bad copy is never trusted;
 * a write always goes to the older copy, so an interrupted write leaves the
 * last good save intact. Values are range-checked after loading. */
#include <string.h>
#include "save.h"

SaveData game;
uint8_t save_status;
static uint8_t last_slot = 1;
static SaveData tmp;

uint16_t save_checksum(const uint8_t *p, uint16_t n) {
    uint16_t a = 0x5A, b = 0xA5;
    while (n--) { a = (uint16_t)((a + *p++) % 255); b = (uint16_t)((b + a) % 255); }
    return (uint16_t)((b << 8) | a);
}

static uint16_t body_len(void) { return (uint16_t)(sizeof(SaveData) - 2); }

uint8_t save_validate(const SaveData *s) {
    uint8_t i;
    if (s->magic != SAVE_MAGIC || s->version != SAVE_VERSION) return 0;
    if (s->checksum != save_checksum((const uint8_t *)s, body_len())) return 0;
    if (s->day < 1 || s->day > 6 || s->map > 15 || s->dir > 3) return 0;
    for (i = 0; i < POND_CELLS; ++i) if (s->pond.pads[i] >= LILY_COUNT) return 0;
    if (s->pond.scale >= SCALE_COUNT || s->pond.density >= DENS_COUNT) return 0;
    return 1;
}

static uint8_t newer(uint8_t a, uint8_t b) { return (uint8_t)(a - b) < 128 && a != b; }

/* Returns slot (0/1) of best valid copy, or 0xFF. */
static uint8_t best_slot(uint8_t *any_data) {
    uint8_t ok0, ok1, seq0 = 0, seq1 = 0;
    sram_read(0, &tmp, sizeof(tmp));
    ok0 = save_validate(&tmp); seq0 = tmp.seq;
    *any_data = tmp.magic == SAVE_MAGIC;
    sram_read(SAVE_SLOT_SIZE, &tmp, sizeof(tmp));
    ok1 = save_validate(&tmp); seq1 = tmp.seq;
    *any_data |= tmp.magic == SAVE_MAGIC;
    if (ok0 && ok1) return newer(seq1, seq0) ? 1 : 0;
    if (ok0) return 0;
    if (ok1) return 1;
    return 0xFF;
}

uint8_t save_probe(void) {
    uint8_t any;
    return best_slot(&any) != 0xFF;
}

uint8_t save_load(void) {
    uint8_t any, slot = best_slot(&any), other_any;
    if (slot == 0xFF) { save_status = any ? SAVE_CORRUPT : SAVE_NONE; return 0; }
    sram_read(slot ? SAVE_SLOT_SIZE : 0, &game, sizeof(game));
    last_slot = slot;
    /* was the other copy newer but damaged? */
    sram_read(slot ? 0 : SAVE_SLOT_SIZE, &tmp, sizeof(tmp));
    other_any = tmp.magic == SAVE_MAGIC;
    save_status = (other_any && !save_validate(&tmp)) ? SAVE_RECOVERED : SAVE_OK;
    return 1;
}

uint8_t save_write(void) {
    uint8_t slot = last_slot ? 0 : 1;
    game.magic = SAVE_MAGIC;
    game.version = SAVE_VERSION;
    ++game.seq;
    game.checksum = save_checksum((const uint8_t *)&game, body_len());
    sram_write(slot ? SAVE_SLOT_SIZE : 0, &game, sizeof(game));
    /* verify by reading back */
    sram_read(slot ? SAVE_SLOT_SIZE : 0, &tmp, sizeof(tmp));
    if (!save_validate(&tmp)) return 0;
    last_slot = slot;
    return 1;
}

void save_erase(void) {
    memset(&tmp, 0, sizeof(tmp));
    sram_write(0, &tmp, sizeof(tmp));
    sram_write(SAVE_SLOT_SIZE, &tmp, sizeof(tmp));
    last_slot = 1;
}

#ifdef __SDCC
#include <gb/gb.h>
void sram_read(uint16_t offset, void *dst, uint16_t n) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy(dst, (const uint8_t *)(0xA000u + offset), n);
    DISABLE_RAM;
}
void sram_write(uint16_t offset, const void *src, uint16_t n) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    memcpy((uint8_t *)(0xA000u + offset), src, n);
    DISABLE_RAM;
}
#endif
