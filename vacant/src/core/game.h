/* VACANT - shared runtime state. */
#ifndef VACANT_GAME_H
#define VACANT_GAME_H
#include "platform.h"

/* facing */
#define F_DOWN 0u
#define F_UP 1u
#define F_LEFT 2u
#define F_RIGHT 3u

/* weather */
#define W_CLEAR 0u
#define W_LIGHT 1u
#define W_HEAVY 2u
#define W_FOG 3u
#define W_STORM 4u
#define WMODE_AUTO 0u     /* weather_mode: 0 auto, 1+W locked */

/* register shadows applied by the VBlank interrupt */
extern volatile uint8_t scx_v, scy_v, wx_v, wy_v, lcdc_win;
extern volatile uint8_t frame_count;

/* player and camera (world metatile coordinates, wrapping uint16) */
extern uint16_t pl_x, pl_y, cam_x, cam_y;
extern uint8_t pl_face, pl_moving, pl_district, pl_listening;
extern uint8_t weather_mode, weather_state;

void player_place(uint16_t x, uint16_t y, uint8_t face);
void player_update(uint8_t keys, uint8_t allow_move);
void player_sprite(void);
void player_hide(void);
void player_refresh_location(void);

void weather_init(void) BANKED;
void weather_update(void) BANKED;
void weather_hide(void) BANKED;

/* interface (bank 3) */
void ui_load_font(void) BANKED;
void popup_show(const char *text) BANKED;
void popup_update(void) BANKED;
void popup_hide(void) BANKED;
uint8_t menu_run(void) BANKED;          /* returns when closed */
uint8_t title_run(void) BANKED;         /* 0 continue, 1 new city */

/* city changes and saving (main.c) */
void city_enter(uint16_t seed, uint8_t use_saved_place);
void fade_out(void);
void fade_in(void);
void game_save_place(void);
uint16_t random_seed(void);
#endif
