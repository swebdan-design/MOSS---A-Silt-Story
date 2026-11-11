/* VACANT - tile graphics and tile animation (ROM bank 2, with its data).
 * The LCD runs in 0x8000 tile addressing, so every upload goes through
 * set_sprite_data (0x8000-based) into the selected VRAM bank. */
#pragma bank 2
#include <gb/gb.h>
#include <gb/cgb.h>
#include "gfx.h"
#include "assets.h"

void gfx_load_world(void) BANKED {
    VBK_REG = VBK_BANK_0;
    set_sprite_data(0, WORLD_TILES_B0, world_gfx_b0);
    VBK_REG = VBK_BANK_1;
    if (WORLD_TILES_B1) set_sprite_data(WORLD_B1_BASE, WORLD_TILES_B1, world_gfx_b1);
    set_sprite_data(0, SPRITE_TILES, sprite_gfx);
    set_sprite_data(TRAIN_BASE, 6, train_gfx);
    VBK_REG = VBK_BANK_0;
}

static uint8_t cur[ANIM_COUNT];

static void show(uint8_t a, uint8_t f) {
    if (cur[a] == f) return;
    cur[a] = f;
    VBK_REG = anim_vram[a << 1];
    set_sprite_data(anim_vram[(a << 1) + 1], 1, &anim_gfx[((uint16_t)a << 6) + ((uint16_t)f << 4)]);
    VBK_REG = VBK_BANK_0;
}

/* Called every frame; at most two tiles change per frame. */
void gfx_animate(uint8_t frame, uint8_t raining) BANKED {
    uint8_t ph = frame & 7u;
    if (ph == 0) show(ANIM_WATER, (uint8_t)((frame >> 4) & 3u));
    else if (ph == 2) show(ANIM_PUDDLE, raining ? (uint8_t)((frame >> 3) & 3u) : 0);
    else if (ph == 4) {
        uint8_t f = (uint8_t)((frame >> 3) & 3u);
        show(ANIM_FAN_TL, f);
        show(ANIM_FAN_TR, f);
    } else if (ph == 5) {
        uint8_t f = (uint8_t)((frame >> 3) & 3u);
        show(ANIM_FAN_BL, f);
        show(ANIM_FAN_BR, f);
    } else if (ph == 6) {
        /* trees lean in the wind now and then */
        uint8_t f = (uint8_t)(((frame >> 5) & 3u) == 1u);
        show(ANIM_TREE_TL, f);
        show(ANIM_TREE_TR, f);
    }
}
