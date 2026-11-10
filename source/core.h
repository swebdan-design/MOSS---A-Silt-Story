/* MOSS — home-bank services: frame loop, input, VRAM, palettes, text, saves. */
#ifndef MOSS_CORE_H
#define MOSS_CORE_H
#include <gb/gb.h>
#include <stdint.h>
#include "compose.h"
#include "mapdef.h"
#include "assets.h"
#include "maps.h"

/* ---------------- VRAM layout (BG uses the 0x8800 addressing mode) ---- */
#define TILE_MAP_FIRST 0    /* map tilesets: 0..143 */
#define TILE_UI 144         /* 16 interface tiles */
#define TILE_VWF 160        /* variable-width text lines, 18 tiles each */
#define VWF_LINE_TILES 18

#define UI_BLANK  (TILE_UI + 0)
#define UI_TL     (TILE_UI + 1)
#define UI_T      (TILE_UI + 2)
#define UI_TR     (TILE_UI + 3)
#define UI_L      (TILE_UI + 4)
#define UI_R      (TILE_UI + 5)
#define UI_BL     (TILE_UI + 6)
#define UI_B      (TILE_UI + 7)
#define UI_BR     (TILE_UI + 8)
#define UI_ARROW  (TILE_UI + 9)
#define UI_MORE   (TILE_UI + 10)
#define UI_BLACK  (TILE_UI + 11)
#define UI_LIGHT  (TILE_UI + 12)
#define UI_DARK   (TILE_UI + 13)
#define UI_DOT    (TILE_UI + 14)
#define UI_NOTE   (TILE_UI + 15)

/* ---------------- input ---------------------------------------------- */
extern uint8_t keys, pressed, released;
extern uint8_t idle_frames;   /* frames since any key (saturating at 255) */

/* ---------------- frame ---------------------------------------------- */
extern uint16_t frame_count;
void frame(void);             /* one display frame: VBlank work, input, music */
void wait_frames(uint8_t n);
void wait_key(uint8_t mask);  /* wait for a fresh press of any key in mask */

/* ---------------- banked memory -------------------------------------- */
void far_copy(void *dst, uint8_t bank, const void *src, uint16_t n);
uint8_t far_byte(uint8_t bank, const uint8_t *src);

void lcd_off(void);
extern uint8_t clip_wx, clip_wy;  /* window's on-screen corner; sprites past it are hidden */
void win_show(uint8_t wx, uint8_t wy);
void win_hide(void);
#define LCD_OFF lcd_off()

/* ---------------- graphics ------------------------------------------- */
extern uint8_t bgp_normal;    /* palette for the current time of day */
void gfx_init(void);
void gfx_set_palette(uint8_t bgp);
void fade_out(void);
void fade_in(void);
void fade_to_black(void);
void gfx_load_map(uint8_t map_id);      /* LCD must be off */
void gfx_reload_map_tiles(void);
void gfx_patch_cell(uint8_t x, uint8_t y, const uint8_t *tiles4);
void gfx_anim_tick(void);
void gfx_load_picture(uint8_t bank, const uint8_t *pic, uint8_t first_tile, uint8_t x, uint8_t y);
void gfx_clear_bg(uint8_t tile);
void gfx_clear_sprites(void);
void gfx_actor(uint8_t slot, uint8_t tile, int16_t x, int16_t y, uint8_t flip, uint8_t pal);
void gfx_hide_actor(uint8_t slot);
void gfx_small(uint8_t sprite, uint8_t tile, int16_t x, int16_t y, uint8_t pal);
void gfx_hide_small(uint8_t sprite);
extern uint8_t cur_map;
extern uint8_t anim_tiles[8][2];
extern uint8_t anim_count;
extern uint8_t anim_pause;

/* ---------------- text ------------------------------------------------ */
extern uint8_t text_speed;    /* 0 slow, 1 normal, 2 fast */
void text_init(void);
void say(const char *s);      /* dialogue box; string may live in the caller's bank */
void say_open(void);
void say_close(void);
uint8_t ask(const char *question, const char *options); /* options "Yes|No"; returns index */
uint8_t menu(const char *items, uint8_t n, uint8_t start, uint8_t width, uint8_t allow_lr);
extern int8_t menu_lr;         /* -1/+1 when left/right pressed in a menu (allow_lr) */
void page_open(void);
void page_close(void);
void page_text(uint8_t line, uint8_t px, const char *s);
void page_center(uint8_t line, const char *s);
void page_wrap(uint8_t line, uint8_t maxlines, const char *s);
void page_icon(uint8_t bank, const uint8_t *pic, uint8_t tx, uint8_t ty);
uint8_t text_width(const char *s);
void vwf_line_clear(uint8_t slot);
void vwf_draw(uint8_t slot, uint8_t px, const char *s);
uint8_t vwf_slot_tile(uint8_t slot);
void num_to_str(char *out, uint16_t v);

#include "save.h"

/* ---------------- misc ------------------------------------------------ */
void apply_settings(void);
#define FLAG(n) (game.flags[(n) >> 3] & (uint8_t)(1u << ((n) & 7)))
#define SET(n) (game.flags[(n) >> 3] |= (uint8_t)(1u << ((n) & 7)))
#define CLR(n) (game.flags[(n) >> 3] &= (uint8_t)~(1u << ((n) & 7)))
#endif
