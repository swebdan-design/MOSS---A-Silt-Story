#ifndef MOSS_MAPDEF_H
#define MOSS_MAPDEF_H
#include <stdint.h>

typedef struct {
    uint8_t w, h, scene, ntiles;
    uint8_t ts_bank;
    const uint8_t *tiles;   /* ntiles * 16 bytes */
    uint8_t mp_bank;
    const uint8_t *map;     /* (2w x 2h) tile ids, then w*h collision flags */
    uint8_t ob_bank;
    const uint8_t *objects; /* warps, objs, listens, patches, anims */
    uint8_t nwarps, nobjs, nlistens, npatches, nanims;
} MapDef;

extern const MapDef map_defs[];

/* collision flags */
#define C_SOLID 1
#define C_WATER 2
#define C_TALL  4
#endif
