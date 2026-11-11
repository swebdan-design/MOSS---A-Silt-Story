/* VACANT - save slots (home bank; no banked data touched while SRAM is on). */
#include "save.h"
#ifndef HOST
#include <gb/gb.h>
#include <gb/hardware.h>
uint8_t *save_sram = (uint8_t *)0xA000u;
void save_sram_on(void) { ENABLE_RAM; SWITCH_RAM(0); }
void save_sram_off(void) { DISABLE_RAM; }
#endif

SaveData save_ram;
uint8_t save_status;

uint16_t save_checksum(const uint8_t *p, uint16_t n) {
    uint16_t a = 0x5A, b = 0xC3;
    while (n--) {
        a += *p++;
        if (a >= 255u) a -= 255u;
        b += a;
        if (b >= 255u) b -= 255u;
    }
    return (uint16_t)((b << 8) | a);
}

#define BODY (uint16_t)(sizeof(SaveData) - 2u)

static uint8_t slot_valid(uint16_t off, SaveData *out) {
    uint8_t *d = (uint8_t *)out;
    uint16_t i;
    for (i = 0; i < sizeof(SaveData); ++i) d[i] = save_sram[off + i];
    if (out->magic != SAVE_MAGIC || out->version != SAVE_VERSION) return 0;
    return save_checksum(d, BODY) == out->sum;
}

void save_defaults(void) {
    uint8_t i, *d = (uint8_t *)&save_ram;
    for (i = 0; i < sizeof(SaveData); ++i) d[i] = 0;
    save_ram.magic = SAVE_MAGIC;
    save_ram.version = SAVE_VERSION;
    save_ram.world_seed = 1;
}

static SaveData tmp;

uint8_t save_load(void) {
    uint8_t a, b;
    save_sram_on();
    a = slot_valid(SLOT_A, &save_ram);
    b = slot_valid(SLOT_B, &tmp);
    save_sram_off();
    if (a && b) {
        /* newest wins (sequence numbers compared modulo 256) */
        if ((uint8_t)(tmp.seq - save_ram.seq) < 128u && tmp.seq != save_ram.seq) save_ram = tmp;
        save_status = SAVE_OK;
    } else if (a) {
        save_status = SAVE_OK;
    } else if (b) {
        save_ram = tmp;
        save_status = SAVE_OK;
    } else {
        save_defaults();
        save_status = SAVE_NONE;
        return save_status;
    }
    /* one slot failed while the other verified: report a recovery */
    if (a != b) {
        /* only a recovery if the damaged slot ever held data */
        uint8_t i, blank = 1;
        uint16_t off = a ? SLOT_B : SLOT_A;
        save_sram_on();
        for (i = 0; i < 4; ++i) if (save_sram[off + i] != 0xFF && save_sram[off + i] != 0x00) blank = 0;
        save_sram_off();
        if (!blank) save_status = SAVE_RECOVERED;
    }
    return save_status;
}

uint8_t save_store(void) {
    uint8_t ok, a, b;
    uint16_t off, i;
    const uint8_t *d;
    save_sram_on();
    a = slot_valid(SLOT_A, &tmp);
    {
        uint8_t seq_a = tmp.seq;
        b = slot_valid(SLOT_B, &tmp);
        /* write over the older (or damaged) slot */
        if (!a) off = SLOT_A;
        else if (!b) off = SLOT_B;
        else off = ((uint8_t)(tmp.seq - seq_a) < 128u) ? SLOT_A : SLOT_B;
        save_ram.seq = (uint8_t)((a && b) ? ((uint8_t)(tmp.seq - seq_a) < 128u ? tmp.seq : seq_a) + 1
                                          : (a ? seq_a + 1 : (b ? tmp.seq + 1 : 1)));
    }
    save_ram.magic = SAVE_MAGIC;
    save_ram.version = SAVE_VERSION;
    save_ram.sum = save_checksum((const uint8_t *)&save_ram, BODY);
    d = (const uint8_t *)&save_ram;
    for (i = 0; i < sizeof(SaveData); ++i) save_sram[off + i] = d[i];
    ok = slot_valid(off, &tmp);
    save_sram_off();
    return ok;
}
