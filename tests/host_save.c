/* Host tests for the save format: fresh, round trip, alternation,
 * interrupted writes, corruption, wrong version, random garbage. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "save.h"

static uint8_t sram[0x2000];
void sram_read(uint16_t o, void *d, uint16_t n) { memcpy(d, sram + o, n); }
void sram_write(uint16_t o, const void *s, uint16_t n) { memcpy(sram + o, s, n); }

static int fails;
#define CHECK(c, m) do { printf("%s: %s\n", (c) ? "PASS" : "FAIL", m); if (!(c)) ++fails; } while (0)

static void fresh_game(uint8_t day) {
    memset(&game, 0, sizeof(game));
    game.day = day; game.map = 1; game.x = 5; game.y = 6; game.pond.bpm = 76;
    game.found = 0x15; game.mmem.interval[3] = 77;
}

int main(void) {
    int i;
    printf("sizeof(SaveData) = %u bytes per copy\n", (unsigned)sizeof(SaveData));
    CHECK(sizeof(SaveData) < SAVE_SLOT_SIZE, "save fits its SRAM slot");
    memset(sram, 0xFF, sizeof(sram));            /* new cartridge: undefined SRAM */
    CHECK(!save_probe() && !save_load(), "blank SRAM -> no save, no crash");
    memset(sram, 0x00, sizeof(sram));
    CHECK(!save_load() && save_status == SAVE_NONE, "zeroed SRAM -> no save");

    fresh_game(2);
    CHECK(save_write(), "write succeeds");
    memset(&game, 0, sizeof(game));
    CHECK(save_load() && game.day == 2 && game.found == 0x15 && game.mmem.interval[3] == 77,
          "round trip restores story and musical memory");

    game.day = 3; save_write();
    game.day = 4; save_write();
    memset(&game, 0, sizeof(game));
    CHECK(save_load() && game.day == 4, "newest of two alternating copies wins");

    /* interrupted write: newest copy half-written */
    game.day = 5; save_write();
    {
        uint8_t newest = (sram[3] > sram[SAVE_SLOT_SIZE + 3]) ? 0 : 1; /* seq byte */
        uint16_t off = newest ? SAVE_SLOT_SIZE : 0;
        memset(sram + off + 20, 0xAA, 40);
    }
    memset(&game, 0, sizeof(game));
    CHECK(save_load() && game.day == 4 && save_status == SAVE_RECOVERED,
          "damaged newest copy -> previous save recovered and reported");

    /* both damaged */
    sram[30] ^= 0x5A; sram[SAVE_SLOT_SIZE + 30] ^= 0x5A;
    CHECK(!save_load() && save_status == SAVE_CORRUPT, "both copies damaged -> refused, reported");

    /* wrong version */
    fresh_game(2); save_write(); save_write();
    {
        SaveData t; memcpy(&t, sram, sizeof(t)); t.version = 9;
        t.checksum = save_checksum((uint8_t *)&t, sizeof(t) - 2); memcpy(sram, &t, sizeof(t));
        memcpy(&t, sram + SAVE_SLOT_SIZE, sizeof(t)); t.version = 9;
        t.checksum = save_checksum((uint8_t *)&t, sizeof(t) - 2); memcpy(sram + SAVE_SLOT_SIZE, &t, sizeof(t));
    }
    CHECK(!save_load(), "unknown version is not loaded");

    /* out-of-range content with a valid checksum */
    fresh_game(2); game.pond.pads[7] = 9; save_write(); save_write();
    CHECK(!save_load(), "valid checksum but impossible values -> rejected");

    /* fuzz */
    srand(1);
    for (i = 0; i < 20000; ++i) {
        int k;
        for (k = 0; k < 0x800; ++k) sram[k] = (uint8_t)rand();
        if (i % 7 == 0) { fresh_game(3); save_write(); sram[rand() % 0x800] ^= (uint8_t)(1 + rand() % 255); }
        save_load();
        if (save_status == SAVE_OK || save_status == SAVE_RECOVERED) {
            if (!(game.day >= 1 && game.day <= 6)) { ++fails; printf("FAIL: fuzz accepted bad data\n"); break; }
        }
    }
    CHECK(fails == 0, "20000 random/corrupted SRAM images never accepted invalid data");

    save_erase();
    CHECK(!save_probe(), "erase leaves no valid save");
    printf("\n%s (%d failures)\n", fails ? "SAVE TESTS FAILED" : "ALL SAVE TESTS PASSED", fails);
    return fails != 0;
}
