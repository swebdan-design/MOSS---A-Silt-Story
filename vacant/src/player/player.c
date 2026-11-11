/* VACANT - the walker, the camera and map streaming (home bank).
 *
 * Movement is grid based: one metatile (16 px) per step, 1 px per frame
 * walking or 2 px per frame with B held. The walker stays at screen
 * metatile (4,4); the background scrolls. At the start of each step the
 * one strip of metatiles about to enter the view is drawn into the ring
 * buffer, so the hardware map never shows a wrapped or missing edge.
 */
#include <gb/gb.h>
#include "../core/game.h"
#include "../world/world.h"
#include "../world/render.h"
#include "../music/music.h"
#include "../graphics/palette.h"
#include "assets.h"

uint16_t pl_x, pl_y, cam_x, cam_y;
uint8_t pl_face, pl_moving, pl_district = 0xFF, pl_listening;
static uint8_t pl_px, pl_dir, pl_parity;

static const int8_t dxs[4] = {0, 0, -1, 1};
static const int8_t dys[4] = {1, -1, 0, 0};
static const char *const dnames[5] = {
    "EMPTY STREETS", "INDUSTRIAL QUARTER", "RESIDENTIAL TOWERS", "LOWER CITY", "FORGOTTEN PLACES"};

static void scroll(void) {
    uint16_t px = cam_x << 4, py = cam_y << 4;
    if (pl_moving) {
        px += (int16_t)dxs[pl_dir] * pl_px;
        py += (int16_t)dys[pl_dir] * pl_px;
    }
    scx_v = (uint8_t)px;
    scy_v = (uint8_t)py;
}

void player_sprite(void) {
    uint8_t f, base, attr = 0x08;   /* palette 0, VRAM bank 1 */
    uint8_t walk = pl_moving && pl_px >= 4 && pl_px < 12;
    uint8_t row = pl_face == F_DOWN ? 0 : pl_face == F_UP ? 3 : 6;
    f = row + (walk ? (pl_parity ? 2 : 1) : 0);
    base = (uint8_t)(f << 2);
    if (pl_face == F_LEFT) {
        attr |= S_FLIPX;
        set_sprite_tile(0, base + 2);
        set_sprite_tile(1, base);
    } else {
        set_sprite_tile(0, base);
        set_sprite_tile(1, base + 2);
    }
    set_sprite_prop(0, attr);
    set_sprite_prop(1, attr);
    move_sprite(0, 72, 80);
    move_sprite(1, 80, 80);
}

void player_hide(void) {
    move_sprite(0, 0, 0);
    move_sprite(1, 0, 0);
}

void player_refresh_location(void) {
    uint8_t d = world_district_at(pl_x, pl_y), dist;
    uint8_t lm = world_landmark(pl_x, pl_y, &dist);
    music_set_location(d, lm, dist);
    pal_district = d;
    if (d != pl_district) {
        if (pl_district != 0xFF && !pl_listening) popup_show(dnames[d]);
        pl_district = d;
    }
}

void player_place(uint16_t x, uint16_t y, uint8_t face) {
    pl_x = x; pl_y = y; pl_face = face;
    cam_x = x - 4; cam_y = y - 4;
    pl_moving = 0;
    render_view(cam_x, cam_y);
    scroll();
    player_sprite();
    pl_district = 0xFF;
    player_refresh_location();
}

static uint8_t try_step(uint8_t dir) {
    uint16_t tx = pl_x + dxs[dir], ty = pl_y + dys[dir];
    pl_face = dir;
    if (world_solid(tx, ty)) return 0;
    switch (dir) {
    case F_RIGHT: render_column(cam_x + 11, cam_y, 10); break;
    case F_LEFT:  render_column(cam_x - 1, cam_y, 10); break;
    case F_DOWN:  render_row(cam_x, cam_y + 10, 11); break;
    default:      render_row(cam_x, cam_y - 1, 11); break;
    }
    pl_dir = dir;
    pl_px = 0;
    pl_moving = 1;
    pl_parity ^= 1;
    return 1;
}

void player_update(uint8_t keys, uint8_t allow_move) {
    if (pl_moving) {
        pl_px += (keys & J_B) ? 2 : 1;
        if (pl_px >= 16) {
            pl_moving = 0;
            pl_x += dxs[pl_dir]; pl_y += dys[pl_dir];
            cam_x += dxs[pl_dir]; cam_y += dys[pl_dir];
            player_refresh_location();
        }
    }
    if (!pl_moving && allow_move) {
        if (keys & J_DOWN) try_step(F_DOWN);
        else if (keys & J_UP) try_step(F_UP);
        else if (keys & J_LEFT) try_step(F_LEFT);
        else if (keys & J_RIGHT) try_step(F_RIGHT);
    }
    scroll();
    player_sprite();
}
