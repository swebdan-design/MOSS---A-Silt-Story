/* VACANT - An Endless City After Dark. by Synth Centric.
 *
 * Home bank: start-up, the VBlank interrupt and the exploration loop.
 *
 * ROM banks: 0 home (this file, interrupt, palette engine, audio, music,
 * saves, player) | 1 city generator | 2 tile graphics and animation |
 * 3 interface, font, title picture | 4 weather | 5 renderer, metatile tables.
 */
#include <gb/gb.h>
#include <gb/cgb.h>
#include <gb/hardware.h>
#include "game.h"
#include "../world/world.h"
#include "../world/render.h"
#include "../graphics/palette.h"
#include "../graphics/gfx.h"
#include "../music/music.h"
#include "../save/save.h"
#include "../ui/ui.h"
#include "assets.h"

volatile uint8_t scx_v, scy_v, wx_v = 7, wy_v = 144, lcdc_win;
volatile uint8_t frame_count;

/* LCD: on, window map 9C00, 0x8000 tile data, BG map 9800, 8x16 sprites,
 * sprites on, BG master priority on. Window enable is bit 5. */
#define LCDC_BASE 0xD7u
#define LCDC_WINON 0x20u

static void vbl_isr(void) {
    SCX_REG = scx_v;
    SCY_REG = scy_v;
    WX_REG = wx_v;
    WY_REG = wy_v;
    LCDC_REG = lcdc_win ? (LCDC_BASE | LCDC_WINON) : LCDC_BASE;
    pal_upload();
    ++frame_count;
    music_tick();
}

/* visual RNG: separate from the geometry and music streams */
static uint16_t vis = 0x3A5Cu;
static uint8_t vis_rand(void) {
    vis ^= vis << 7;
    vis ^= vis >> 9;
    vis ^= vis << 8;
    return (uint8_t)vis;
}

uint16_t random_seed(void) {
    uint16_t s = ((uint16_t)DIV_REG << 8) ^ frame_count ^ (uint16_t)(vis_rand() << 4) ^ music_steps;
    vis ^= s;
    return s ? s : 1;
}

void fade_out(void) {
    while (pal_fade < 16) {
        wait_vbl_done();
        pal_fade += 2;
        if (pal_fade > 16) pal_fade = 16;
        pal_update();
    }
    wait_vbl_done();
    wait_vbl_done();
}

void fade_in(void) {
    while (pal_fade > 0) {
        wait_vbl_done();
        pal_fade = pal_fade > 2 ? pal_fade - 2 : 0;
        pal_update();
    }
}

/* nearest walkable metatile, searching outward in rings */
static void nearest_walkable(uint16_t *x, uint16_t *y) {
    uint8_t r;
    int8_t i;
    if (!world_solid(*x, *y)) return;
    for (r = 1; r < 24; ++r)
        for (i = -(int8_t)r; i <= (int8_t)r; ++i) {
            if (!world_solid(*x + i, *y - r)) { *x += i; *y -= r; return; }
            if (!world_solid(*x + i, *y + r)) { *x += i; *y += r; return; }
            if (!world_solid(*x - r, *y + i)) { *x -= r; *y += i; return; }
            if (!world_solid(*x + r, *y + i)) { *x += r; *y += i; return; }
        }
}

/* Enter a city: the change is hidden inside a short fade to black, during
 * which the music keeps playing. */
void city_enter(uint16_t seed, uint8_t use_saved_place) {
    uint16_t x, y;
    uint8_t face = F_DOWN;
    fade_out();
    player_hide();
    weather_hide();
    popup_hide();
    train_active = 0;
    world_set_seed(seed);
    if (use_saved_place && save_ram.has_place && save_ram.world_seed == seed) {
        x = save_ram.px;
        y = save_ram.py;
        face = save_ram.face & 3;
        nearest_walkable(&x, &y);
    } else {
        world_find_start(&x, &y);
    }
    player_place(x, y, face);
    pal_snap_district();
    fade_in();
}

void game_save_place(void) {
    uint8_t i;
    save_ram.has_place = 1;
    save_ram.world_seed = world_seed;
    save_ram.px = pl_x;
    save_ram.py = pl_y;
    save_ram.face = pl_face;
    save_ram.music_seed = music_seed;
    for (i = 0; i < NPARAM; ++i) save_ram.params[i] = mparam[i];
    save_ram.weather_mode = weather_mode;
    save_ram.freeze = music_freeze;
    if (!save_store()) save_ram.has_place = 0;
}

static void apply_saved_settings(void) {
    uint8_t i;
    for (i = 0; i < NPARAM; ++i) {
        uint8_t v = save_ram.params[i];
        mparam[i] = v > param_max[i] ? param_max[i] : v;
    }
    weather_mode = save_ram.weather_mode < 6 ? save_ram.weather_mode : WMODE_AUTO;
    music_freeze = save_ram.freeze & 1;
}

static void hardware_init(void) {
    uint8_t i;
    DISPLAY_OFF;
    LCDC_REG = LCDC_BASE & 0x7Fu;
    pal_init();
    pal_update();
    pal_dirty = 0;
    {
        /* upload the (black) palettes before the screen comes on */
        const uint8_t *p = (const uint8_t *)pal_out;
        BCPS_REG = 0x80;
        for (i = 0; i < 64; ++i) BCPD_REG = p[i];
        OCPS_REG = 0x80;
        for (i = 64; i < 96; ++i) OCPD_REG = p[i];
    }
    gfx_load_world();
    ui_load_font();
    for (i = 0; i < 40; ++i) move_sprite(i, 0, 0);
}

void main(void) {
    uint8_t keys, prev = 0, pressed, r;
    if (_cpu != CGB_TYPE) {
        DISPLAY_OFF;
        ui_dmg_notice();
    }
    cpu_fast();
    hardware_init();

    /* the synthesiser starts from the saved settings, or the first preset */
    music_load_preset(0);
    mparam[P_ATMOS] = 8;
    mparam[P_INFLUENCE] = 10;
    weather_mode = WMODE_AUTO;
    if (save_load() != SAVE_NONE && save_ram.has_place) {
        apply_saved_settings();
        music_seed = save_ram.music_seed;
    } else {
        music_seed = 0x7A31u;
    }
    music_init(music_seed);

    CRITICAL {
        add_VBL(vbl_isr);
    }
    set_interrupts(VBL_IFLAG);
    enable_interrupts();
    LCDC_REG = LCDC_BASE;

    r = title_run();
    /* title used the tile memory: restore the city graphics (display stays on) */
    gfx_load_world();
    ui_load_font();
    pal_fade = 16;
    pal_update();
    weather_init();
    if (r == 0) city_enter(save_ram.world_seed, 1);
    else city_enter(random_seed(), 0);
    if (save_status == SAVE_RECOVERED) popup_show("SAVE RECOVERED");

    while (1) {
        wait_vbl_done();
        keys = joypad();
        pressed = keys & (uint8_t)~prev;
        prev = keys;

        if ((pressed & J_START) && !pl_moving) {
            player_hide();
            weather_hide();
            r = menu_run();
            pal_update();
            if (r == MENU_CITY) {
                city_enter(menu_seed, 0);
            } else if (r == MENU_RETURN) {
                save_load();
                if (save_ram.has_place) {
                    city_enter(save_ram.world_seed, 1);
                    popup_show("RETURNED");
                }
            } else {
                player_sprite();
            }
            prev = joypad();
            continue;
        }
        if (pressed & J_SELECT) {
            pl_listening ^= 1;
            if (pl_listening) {
                pl_face = F_DOWN;
                popup_show("LISTENING");
            } else {
                popup_hide();
            }
        }
        if ((pressed & J_A) && !pl_listening) {
            music_focus ^= 1;
            pal_dim = music_focus ? 3 : 0;
            player_refresh_location();
            popup_show(music_focus ? "FOCUS" : "FOCUS OFF");
        }

        player_update(keys, !pl_listening);
        render_blink_update(frame_count, mparam[P_ATMOS]);
        gfx_animate(frame_count, music_rain > 2);
        weather_update();
        if (frame_count == 0 && vis_rand() < (uint8_t)(20 + mparam[P_ATMOS] * 6))
            train_try_start(cam_x, cam_y, vis_rand() & 1);
        train_update(cam_x, cam_y);
        popup_update();
        pal_update();
    }
}
