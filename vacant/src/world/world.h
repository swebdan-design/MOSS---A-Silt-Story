/* VACANT - deterministic procedural city.
 *
 * The world is a torus of 65536 x 65536 metatiles (16x16 px each). It is
 * divided into superblocks of 64x64 metatiles, each split by four jittered
 * street lines per axis into city cells ("blocks") of 12..20 metatiles.
 * A cell owns its west road, its north road and everything inside them.
 * Every metatile is a pure function of (world seed, x, y): cells are
 * generated on demand into a small cache and can be rebuilt at any time
 * in any order with identical results.
 */
#ifndef VACANT_WORLD_H
#define VACANT_WORLD_H
#include "../core/platform.h"

#define CELL_MAX 20u
#define CELL_CACHE 6u

#define D_STREETS 0u
#define D_INDUSTRIAL 1u
#define D_RESIDENTIAL 2u
#define D_LOWER 3u
#define D_FORGOTTEN 4u
#define D_COUNT 5u

/* acoustic landmarks */
#define LM_NONE 0u
#define LM_NEON 1u        /* streets: big neon sign, electrical buzz      */
#define LM_SUBSTATION 2u  /* industrial: regular low-frequency pulse      */
#define LM_VENTS 3u       /* residential: rooftop fans, slow cycle        */
#define LM_PLATFORM 4u    /* lower city: abandoned platform, intervals    */
#define LM_GARDEN 5u      /* forgotten: quiet garden, delicate bells      */

typedef struct {
    uint16_t cx, cy;       /* cell index (0..4095 each)          */
    uint16_t x0, y0;       /* world metatile of local (0,0)      */
    uint8_t w, h;          /* size in metatiles                  */
    uint8_t district;
    uint8_t lm_type, lm_x, lm_y;
    uint8_t valid, stamp;
    uint8_t fixes;         /* walkable pockets repaired (stats)  */
    uint8_t mt[CELL_MAX * CELL_MAX];
} Cell;

extern uint16_t world_seed;
extern uint8_t world_last_fixes;

void world_set_seed(uint16_t seed) BANKED;
uint8_t world_mt(uint16_t x, uint16_t y) BANKED;
uint8_t world_solid(uint16_t x, uint16_t y) BANKED;
uint8_t world_district_at(uint16_t x, uint16_t y) BANKED;
/* nearest landmark (within the cached cells) to (x,y): type, distance */
uint8_t world_landmark(uint16_t x, uint16_t y, uint8_t *dist) BANKED;
void world_find_start(uint16_t *x, uint16_t *y) BANKED;
uint8_t world_is_viaduct_row(uint16_t y) BANKED;
/* cell lookup (host tests, map tool) */
Cell *world_cell_at(uint16_t x, uint16_t y) BANKED;
uint8_t world_cell_district(uint16_t cx, uint16_t cy) BANKED;

#endif
