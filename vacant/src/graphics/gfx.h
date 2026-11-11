/* VACANT - tile graphics loading and tile animation (ROM bank 2). */
#ifndef VACANT_GFX_H
#define VACANT_GFX_H
#include "../core/platform.h"

void gfx_load_world(void) BANKED;
void gfx_animate(uint8_t frame, uint8_t raining) BANKED;
#endif
