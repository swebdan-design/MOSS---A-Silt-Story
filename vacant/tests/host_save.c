/* Host tests for VACANT saves: blank cartridge, alternating slots, a write
 * interrupted part-way (simulated power cut), corruption of either slot,
 * sequence-number wrap-around and preset storage. */
#include <stdio.h>
#include <string.h>
#include "../src/save/save.h"

static uint8_t sram[8192];
uint8_t *save_sram = sram;
void save_sram_on(void) {}
void save_sram_off(void) {}

static int failures;
#define CHECK(c, ...) do { if (!(c)) { ++failures; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    int i;
    memset(sram, 0xFF, sizeof sram);               /* fresh battery RAM */
    CHECK(save_load() == SAVE_NONE, "blank SRAM not reported as empty");
    CHECK(save_ram.world_seed == 1, "defaults not applied");

    save_ram.has_place = 1; save_ram.world_seed = 4242; save_ram.px = 100; save_ram.py = 200;
    CHECK(save_store(), "first store not verified");
    save_ram.px = 101;
    CHECK(save_store(), "second store not verified");
    memset(&save_ram, 0, sizeof save_ram);
    CHECK(save_load() == SAVE_OK && save_ram.px == 101, "newest slot not chosen (px=%u)", save_ram.px);

    /* power cut while writing the next save: half of the target slot written */
    {
        uint8_t backup[sizeof(SaveData)];
        uint16_t target;
        save_ram.px = 102;
        /* the next write goes to the slot not holding px=101 */
        target = (sram[SLOT_A + 3] < sram[SLOT_B + 3]) ? SLOT_A : SLOT_B;
        if (sram[SLOT_A + 3] == sram[SLOT_B + 3]) target = SLOT_A;
        memcpy(backup, &sram[target], sizeof backup);
        save_store();
        memcpy(&sram[target + sizeof(SaveData) / 2], backup + sizeof(SaveData) / 2, sizeof(SaveData) / 2);
        memset(&save_ram, 0, sizeof save_ram);
        save_load();
        CHECK(save_ram.px == 101 || save_ram.px == 102, "interrupted write lost both copies (px=%u)", save_ram.px);
        printf("power cut mid-write: recovered px=%u (status %u)\n", save_ram.px, save_status);
    }

    /* corrupt one slot entirely: the other is used and reported */
    save_ram.px = 300; save_store();
    save_ram.px = 301; save_store();
    sram[SLOT_A + 10] ^= 0x5A;
    sram[SLOT_B + 10] ^= 0x00;
    memset(&save_ram, 0, sizeof save_ram);
    CHECK(save_load() != SAVE_NONE, "single corrupted slot lost everything");
    printf("one slot corrupted: status %u px=%u\n", save_status, save_ram.px);

    /* both corrupted: defaults, no crash */
    sram[SLOT_B + 12] ^= 0xA5;
    CHECK(save_load() == SAVE_NONE, "two corrupted slots accepted");

    /* sequence wrap-around */
    memset(sram, 0xFF, sizeof sram);
    save_load();
    for (i = 0; i < 600; ++i) { save_ram.px = (uint16_t)i; CHECK(save_store(), "store %d failed", i); }
    memset(&save_ram, 0, sizeof save_ram);
    save_load();
    CHECK(save_ram.px == 599, "after 600 saves loaded px=%u", save_ram.px);

    /* presets persist */
    save_ram.preset_used = 0x05;
    save_ram.presets[2][P_MODE] = 6;
    save_ram.preset_seed[2] = 0xBEEF;
    save_store();
    memset(&save_ram, 0, sizeof save_ram);
    save_load();
    CHECK(save_ram.preset_used == 0x05 && save_ram.presets[2][P_MODE] == 6 && save_ram.preset_seed[2] == 0xBEEF,
          "user preset not persisted");
    printf("save record: %u bytes per slot\n", (unsigned)sizeof(SaveData));
    printf(failures ? "host_save: %d FAILURES\n" : "host_save: all checks passed\n", failures);
    return failures ? 1 : 0;
}
