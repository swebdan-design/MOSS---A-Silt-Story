/* MOSS — frame loop, input, banked copies, VRAM and sprites (home bank). */
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"

uint8_t keys, pressed, released, idle_frames;
uint16_t frame_count;
uint8_t bgp_normal = 0xE4;
uint8_t cur_map = 0xFF;
uint8_t anim_tiles[8][2];
uint8_t anim_count;
uint8_t anim_pause;   /* set while pages borrow map tile slots */
uint8_t scroll_x, scroll_y;
uint8_t clip_wx = 255, clip_wy = 255;
static uint8_t prev_keys;

extern const uint8_t ui_tiles[];

/* ---------------------------------------------------------------- input/frame */
void frame(void) {
    wait_vbl_done();
    SCX_REG = scroll_x;
    SCY_REG = scroll_y;
    if ((frame_count & 15) == 0) gfx_anim_tick();
    prev_keys = keys;
    keys = joypad();
    pressed = keys & (uint8_t)~prev_keys;
    released = prev_keys & (uint8_t)~keys;
    if (keys) idle_frames = 0;
    else if (idle_frames < 255) ++idle_frames;
    compose_frame();
    ++frame_count;
}

void wait_frames(uint8_t n) { while (n--) frame(); }

void wait_key(uint8_t mask) {
    frame();
    while (!(pressed & mask)) frame();
}

/* ---------------------------------------------------------------- banks */
void far_copy(void *dst, uint8_t bank, const void *src, uint16_t n) {
    uint8_t save = CURRENT_BANK;
    SWITCH_ROM(bank);
    memcpy(dst, src, n);
    SWITCH_ROM(save);
}

uint8_t far_byte(uint8_t bank, const uint8_t *src) {
    uint8_t save = CURRENT_BANK, v;
    SWITCH_ROM(bank);
    v = *src;
    SWITCH_ROM(save);
    return v;
}

/* BG tiles in 0x8800 mode: 0..127 at 0x9000, 128..255 at 0x8800. */
static void bg_tiles(uint8_t first, uint8_t n, const uint8_t *data) {
    while (n) {
        uint8_t *addr = first < 128 ? (uint8_t *)(0x9000u + (uint16_t)first * 16u)
                                    : (uint8_t *)(0x8800u + (uint16_t)(first - 128) * 16u);
        uint8_t run = first < 128 ? (uint8_t)(128 - first) : (uint8_t)(256 - first);
        if (run > n) run = n;
        set_data(addr, data, (uint16_t)run * 16u);
        data += (uint16_t)run * 16u;
        first += run;
        n -= run;
    }
}

void gfx_bg_tiles(uint8_t first, uint8_t n, const uint8_t *data) { bg_tiles(first, n, data); }

/* ---------------------------------------------------------------- palettes */
static uint8_t lighten(uint8_t pal, uint8_t steps) {
    uint8_t out = 0, i, s;
    for (i = 0; i < 4; ++i) {
        s = (pal >> (i * 2)) & 3;
        s = s > steps ? s - steps : 0;
        out |= (uint8_t)(s << (i * 2));
    }
    return out;
}
static uint8_t darken(uint8_t pal, uint8_t steps) {
    uint8_t out = 0, i, s;
    for (i = 0; i < 4; ++i) {
        s = (pal >> (i * 2)) & 3;
        s = s + steps > 3 ? 3 : s + steps;
        out |= (uint8_t)(s << (i * 2));
    }
    return out;
}

#define OBP0_NORMAL 0xE0  /* 1 white, 2 dark grey, 3 black */
#define OBP1_NORMAL 0xD0  /* 1 white, 2 light grey, 3 black */

void gfx_set_palette(uint8_t bgp) {
    BGP_REG = bgp;
    OBP0_REG = OBP0_NORMAL;
    OBP1_REG = OBP1_NORMAL;
}

void fade_out(void) {
    uint8_t i;
    for (i = 1; i <= 3; ++i) {
        BGP_REG = lighten(bgp_normal, i);
        OBP0_REG = lighten(OBP0_NORMAL, i);
        OBP1_REG = lighten(OBP1_NORMAL, i);
        wait_frames(5);
    }
    BGP_REG = 0; OBP0_REG = 0; OBP1_REG = 0;
    frame();
}

void fade_in(void) {
    uint8_t i;
    for (i = 3; i >= 1; --i) {
        BGP_REG = lighten(bgp_normal, i);
        OBP0_REG = lighten(OBP0_NORMAL, i);
        OBP1_REG = lighten(OBP1_NORMAL, i);
        wait_frames(5);
    }
    gfx_set_palette(bgp_normal);
    frame();
}

void fade_to_black(void) {
    uint8_t i;
    for (i = 1; i <= 3; ++i) {
        BGP_REG = darken(BGP_REG, 1);
        OBP0_REG = darken(OBP0_REG, 1);
        OBP1_REG = darken(OBP1_REG, 1);
        wait_frames(8);
    }
}

/* Turn the LCD off only during VBlank, with interrupts held off so the
   wait can never miss the window. Safe to call when it's already off. */
void lcd_off(void) {
    if (!(LCDC_REG & LCDCF_ON)) return;
    __critical {
        while (LY_REG < 144) ;
        LCDC_REG &= (uint8_t)~LCDCF_ON;
    }
}

/* ---------------------------------------------------------------- maps */
void gfx_init(void) {
    LCD_OFF;
    LCDC_REG = LCDCF_OFF | LCDCF_BG8800 | LCDCF_BG9800 | LCDCF_WIN9C00 | LCDCF_OBJ16 |
               LCDCF_OBJON | LCDCF_BGON;
    bg_tiles(TILE_UI, 16, ui_tiles);
    gfx_clear_bg(UI_BLANK);
    gfx_clear_sprites();
    gfx_set_palette(bgp_normal);
}

static uint8_t row_buf[32];

void gfx_clear_bg(uint8_t tile) {
    uint8_t y;
    memset(row_buf, tile, 32);
    for (y = 0; y < 32; ++y) set_bkg_tiles(0, y, 32, 1, row_buf);
}

void gfx_load_map(uint8_t map_id) {
    const MapDef *m = &map_defs[map_id];
    uint8_t save = CURRENT_BANK, y, w2 = m->w * 2, h2 = m->h * 2, i;
    cur_map = map_id;
    SWITCH_ROM(m->ts_bank);
    bg_tiles(TILE_MAP_FIRST, m->ntiles, m->tiles);
    SWITCH_ROM(m->mp_bank);
    for (y = 0; y < h2; ++y) set_bkg_tiles(0, y, w2, 1, m->map + (uint16_t)y * w2);
    SWITCH_ROM(m->ob_bank);
    {
        /* animation list sits at the end of the object block */
        const uint8_t *p = m->objects + (uint16_t)m->nwarps * 6 + (uint16_t)m->nobjs * 3 +
                           (uint16_t)m->nlistens * 5 + (uint16_t)m->npatches * 8;
        anim_count = m->nanims > 8 ? 8 : m->nanims;
        for (i = 0; i < anim_count; ++i) { anim_tiles[i][0] = p[i * 2]; anim_tiles[i][1] = p[i * 2 + 1]; }
    }
    SWITCH_ROM(save);
}

void gfx_reload_map_tiles(void) {
    const MapDef *m = &map_defs[cur_map];
    uint8_t save = CURRENT_BANK;
    if (cur_map == 0xFF) return;
    SWITCH_ROM(m->ts_bank);
    bg_tiles(TILE_MAP_FIRST, m->ntiles, m->tiles);
    SWITCH_ROM(save);
}

void gfx_patch_cell(uint8_t x, uint8_t y, const uint8_t *t) {
    set_bkg_tiles(x * 2, y * 2, 2, 1, t);
    set_bkg_tiles(x * 2, y * 2 + 1, 2, 1, t + 2);
}

void gfx_anim_tick(void) {
    uint8_t i, f = (uint8_t)((frame_count >> 4) & 3);
    if (anim_pause) return;
    for (i = 0; i < anim_count; ++i) {
        bg_tiles(anim_tiles[i][0], 1, anim_frames + (uint16_t)anim_tiles[i][1] * 64u + (uint16_t)f * 16u);
    }
}

/* Picture blob: w, h, ntiles, tiles[ntiles*16], map[w*h] */
void gfx_load_picture(uint8_t bank, const uint8_t *pic, uint8_t first_tile, uint8_t x, uint8_t y) {
    uint8_t save = CURRENT_BANK, w, h, n, r, c;
    const uint8_t *map;
    SWITCH_ROM(bank);
    w = pic[0]; h = pic[1]; n = pic[2];
    bg_tiles(first_tile, n, pic + 3);
    map = pic + 3 + (uint16_t)n * 16u;
    for (r = 0; r < h; ++r) {
        for (c = 0; c < w; ++c) row_buf[c] = (uint8_t)(map[(uint16_t)r * w + c] + first_tile);
        set_bkg_tiles(x, y + r, w, 1, row_buf);
    }
    SWITCH_ROM(save);
}

/* Same, but onto the window layer (used by notebook pages). */
void gfx_load_picture_win(uint8_t bank, const uint8_t *pic, uint8_t first_tile, uint8_t x, uint8_t y) {
    uint8_t save = CURRENT_BANK, w, h, n, r, c;
    const uint8_t *map;
    SWITCH_ROM(bank);
    w = pic[0]; h = pic[1]; n = pic[2];
    bg_tiles(first_tile, n, pic + 3);
    map = pic + 3 + (uint16_t)n * 16u;
    for (r = 0; r < h; ++r) {
        for (c = 0; c < w; ++c) row_buf[c] = (uint8_t)(map[(uint16_t)r * w + c] + first_tile);
        set_win_tiles(x, y + r, w, 1, row_buf);
    }
    SWITCH_ROM(save);
}

/* ---------------------------------------------------------------- sprites */
void gfx_load_sprites(void) {
    uint8_t save = CURRENT_BANK;
    SWITCH_ROM(BANK_spr_tiles);
    set_sprite_data(0, SPR_TILE_COUNT, spr_tiles);
    SWITCH_ROM(save);
}

void gfx_clear_sprites(void) {
    uint8_t i;
    for (i = 0; i < 40; ++i) { move_sprite(i, 0, 0); set_sprite_prop(i, 0); }
}

/* Hardware sprites are drawn above the window layer, so anything that
   would overlap an open text box or menu is hidden instead. */
static uint8_t clipped(int16_t x, int16_t y, uint8_t w) {
    return (x + w > (int16_t)clip_wx) && (y + 16 > (int16_t)clip_wy);
}

void win_show(uint8_t wx, uint8_t wy) {
    uint8_t i;
    move_win(wx, wy);
    clip_wx = wx - 7; clip_wy = wy;
    for (i = 0; i < 40; ++i) {
        uint8_t sy = shadow_OAM[i].y, sx = shadow_OAM[i].x;
        if (sy && sx + 0 > clip_wx && sy > clip_wy) shadow_OAM[i].y = 0;
    }
    SHOW_WIN;
}

void win_hide(void) {
    HIDE_WIN;
    clip_wx = 255; clip_wy = 255;
}

/* 16x16 actor in two 8x16 hardware sprites: slot uses sprites 2*slot, 2*slot+1 */
void gfx_actor(uint8_t slot, uint8_t tile, int16_t x, int16_t y, uint8_t flip, uint8_t pal) {
    uint8_t a = slot * 2, b = a + 1, prop = pal ? S_PALETTE : 0;
    if (x < -16 || x > 168 || y < -16 || y > 160 || clipped(x, y, 16)) { gfx_hide_actor(slot); return; }
    if (flip) {
        set_sprite_tile(a, tile + 2); set_sprite_tile(b, tile);
        prop |= S_FLIPX;
    } else {
        set_sprite_tile(a, tile); set_sprite_tile(b, tile + 2);
    }
    set_sprite_prop(a, prop); set_sprite_prop(b, prop);
    move_sprite(a, (uint8_t)(x + 8), (uint8_t)(y + 16));
    move_sprite(b, (uint8_t)(x + 16), (uint8_t)(y + 16));
}

void gfx_hide_actor(uint8_t slot) {
    move_sprite(slot * 2, 0, 0);
    move_sprite(slot * 2 + 1, 0, 0);
}

void gfx_small(uint8_t sprite, uint8_t tile, int16_t x, int16_t y, uint8_t pal) {
    if (x < -8 || x > 168 || y < -16 || y > 160 || clipped(x, y, 8)) { move_sprite(sprite, 0, 0); return; }
    set_sprite_tile(sprite, tile);
    set_sprite_prop(sprite, pal ? S_PALETTE : 0);
    move_sprite(sprite, (uint8_t)(x + 8), (uint8_t)(y + 16));
}

void gfx_hide_small(uint8_t sprite) { move_sprite(sprite, 0, 0); }
