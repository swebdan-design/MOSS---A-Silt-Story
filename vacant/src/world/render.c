/* VACANT - city renderer, blinking lights and passing trains (ROM bank 5). */
#pragma bank 5
#include <gb/gb.h>
#include <gb/cgb.h>
#include "world.h"
#include "render.h"
#include "assets.h"
#include "../music/music.h"

/* ------------------------------------------------------------ blinkers */
#define NBLINK 14u
#define BK_TRAFFIC 1u
#define BK_LAMP 2u
#define BK_PULSE 3u
#define BK_SIGN 4u

typedef struct {
    uint8_t kind, on, timer, mt;
    uint16_t wx, wy;
} Blink;

static Blink blk[NBLINK];
static uint8_t abuf[4];
static uint16_t vrs = 0x1D3Bu;   /* visual RNG stream: independent of geometry */

static uint8_t vrnd(void) {
    vrs ^= vrs << 7;
    vrs ^= vrs >> 9;
    vrs ^= vrs << 8;
    return (uint8_t)vrs;
}

static void write_attrs(uint16_t wx, uint16_t wy, const uint8_t *a) {
    uint8_t mx = (uint8_t)((wx & 15u) << 1), my = (uint8_t)((wy & 15u) << 1);
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(mx, my, 2, 2, a);
    VBK_REG = VBK_TILES;
}

static void put_mt(uint16_t wx, uint16_t wy, uint8_t m) {
    uint8_t mx = (uint8_t)((wx & 15u) << 1), my = (uint8_t)((wy & 15u) << 1);
    uint16_t o = (uint16_t)m << 2;
    uint8_t i, kind = 0;
    VBK_REG = VBK_ATTRIBUTES;
    set_bkg_tiles(mx, my, 2, 2, &mt_attrs[o]);
    VBK_REG = VBK_TILES;
    set_bkg_tiles(mx, my, 2, 2, &mt_tiles[o]);
    /* this map slot now shows something else: forget old blinkers there */
    for (i = 0; i < NBLINK; ++i)
        if (blk[i].kind && ((blk[i].wx ^ wx) & 15u) == 0 && ((blk[i].wy ^ wy) & 15u) == 0)
            blk[i].kind = 0;
    if (!(rmt_flags[m] & MTF_BLINK)) return;
    if (m == MT_TRAFFIC) kind = BK_TRAFFIC;
    else if (m == MT_LAMP_FLICKER) kind = BK_LAMP;
    else if (m == MT_ROOF_BEACON || m == MT_WALL_WARN || m == MT_TRANSFORMER) kind = BK_PULSE;
    else kind = BK_SIGN;
    for (i = 0; i < NBLINK; ++i) {
        if (!blk[i].kind) {
            blk[i].kind = kind; blk[i].on = 1; blk[i].mt = m;
            blk[i].wx = wx; blk[i].wy = wy;
            blk[i].timer = vrnd();
            return;
        }
    }
}

static void set_lit(Blink *b, uint8_t on) {
    uint16_t o = (uint16_t)b->mt << 2;
    uint8_t i, alt = rmt_flags[b->mt] >> 4;
    if (b->on == on) return;
    b->on = on;
    for (i = 0; i < 4; ++i) {
        uint8_t a = mt_attrs[o + i];
        abuf[i] = on ? a : (uint8_t)((a & 0xF8u) | alt);
    }
    write_attrs(b->wx, b->wy, abuf);
}

void render_blink_reset(void) BANKED {
    uint8_t i;
    for (i = 0; i < NBLINK; ++i) blk[i].kind = 0;
}

/* activity 0..15 (the ATMOSPHERE setting) scales how restless lights are */
void render_blink_update(uint8_t frame, uint8_t activity) BANKED {
    uint8_t i;
    for (i = 0; i < NBLINK; ++i) {
        Blink *b = &blk[i];
        switch (b->kind) {
        case BK_TRAFFIC:   /* night mode: slow amber-red blink, all in step */
            set_lit(b, (frame & 0x40u) != 0);
            break;
        case BK_PULSE:     /* warning lights: brief flash every ~1.5 s */
            set_lit(b, ((uint8_t)(frame + (uint8_t)(b->wx << 3)) & 0x5Fu) < 0x0Cu);
            break;
        case BK_LAMP:      /* a failing lamp: bursts of flicker, then long calm */
            if (b->timer) { --b->timer; break; }
            if (b->on) {
                set_lit(b, 0);
                b->timer = 2 + (vrnd() & 7);
            } else {
                set_lit(b, 1);
                b->timer = (vrnd() < 110) ? (uint8_t)(3 + (vrnd() & 7))
                                          : (uint8_t)(60 + (vrnd() >> (activity >> 2)));
            }
            break;
        case BK_SIGN:      /* unstable neon: now and then it dies for a while */
            if ((uint8_t)(frame & 63u) == (uint8_t)((i << 2) & 63u)) {
                if (b->on) {
                    if (vrnd() < (uint8_t)(4 + activity * 2)) {
                        set_lit(b, 0);
                        b->timer = 2 + (vrnd() & 7);
                        music_event(EV_BUZZ);
                    }
                } else if (--b->timer == 0) {
                    set_lit(b, 1);
                    music_event(EV_BUZZ);
                }
            } else if (!b->on && b->timer == 1) {
                set_lit(b, (frame & 6u) == 6u);   /* stutters before it returns */
            }
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------- drawing */
void render_column(uint16_t x, uint16_t y0, uint8_t n) BANKED {
    uint8_t i;
    for (i = 0; i < n; ++i) put_mt(x, (uint16_t)(y0 + i), world_mt(x, (uint16_t)(y0 + i)));
}

void render_row(uint16_t x0, uint16_t y, uint8_t n) BANKED {
    uint8_t i;
    for (i = 0; i < n; ++i) put_mt((uint16_t)(x0 + i), y, world_mt((uint16_t)(x0 + i), y));
}

void render_view(uint16_t cx, uint16_t cy) BANKED {
    uint8_t j;
    render_blink_reset();
    for (j = 0; j < 10; ++j) render_row(cx, (uint16_t)(cy + j), 11);
}

/* -------------------------------------------------------------- trains
 * A train is painted into the two tile rows of the viaduct deck that hold
 * the track (bottom half of VIA_T, top half of VIA_B) and moved one tile
 * every three frames. When it has passed, the deck is redrawn. */
uint8_t train_active;
static uint16_t tr_y;          /* world metatile row of VIA_T            */
static int16_t tr_head;        /* head position in tiles, view-relative  */
static int8_t tr_dir;
static uint8_t tr_len, tr_tick;
static uint8_t trow_t[24], trow_a[24];

static uint8_t train_tile(int16_t k) {
    /* k = 0 at the nose, increasing toward the tail; car = 9 tiles */
    uint8_t c;
    if (k == 0 || k == tr_len - 1) return 0;   /* nose / tail */
    c = (uint8_t)(k % 9);
    return c == 0 ? 2 : 1;                       /* coupling gap / body */
}

uint8_t train_try_start(uint16_t cam_x, uint16_t cam_y, uint8_t dir) BANKED {
    uint8_t j;
    if (train_active) return 0;
    for (j = 0; j < 8; ++j) {
        uint16_t y = cam_y + j;
        if (world_mt(cam_x, y) == MT_VIA_T || world_mt(cam_x + 5, y) == MT_VIA_T ||
            world_mt(cam_x, y) == MT_VIA_T_OVER) {
            tr_y = y;
            tr_dir = dir ? 1 : -1;
            tr_len = 9 * 3 + 1;
            tr_head = dir ? -1 : 22;
            tr_tick = 0;
            train_active = 1;
            music_event(EV_TRAIN);
            return 1;
        }
    }
    return 0;
}

static void train_restore(uint16_t cam_x) {
    render_row(cam_x, tr_y, 11);
    render_row(cam_x, tr_y + 1, 11);
}

void train_update(uint16_t cam_x, uint16_t cam_y) BANKED {
    uint8_t i, mx0, ty;
    int16_t k;
    if (!train_active) return;
    if ((uint16_t)(tr_y - cam_y) > 8u) { train_active = 0; return; }  /* left the view */
    if (++tr_tick < 3) return;
    tr_tick = 0;
    tr_head += tr_dir;
    if ((tr_dir > 0 && tr_head - tr_len > 22) || (tr_dir < 0 && tr_head + tr_len < -1)) {
        train_active = 0;
        train_restore(cam_x);
        return;
    }
    /* build the 22 visible tiles of each track row */
    for (i = 0; i < 22; ++i) {
        k = tr_dir > 0 ? (tr_head - (int16_t)i) : ((int16_t)i - tr_head);
        if (k >= 0 && k < tr_len) {
            uint8_t t = train_tile(k);
            uint8_t flip = (tr_dir > 0) ? 0x20u : 0;
            if (k == tr_len - 1) flip ^= 0x20u;
            trow_t[i] = TRAIN_BASE + t;
            trow_a[i] = PAL_LAMP | 0x08u | flip;
        } else {
            trow_t[i] = 0xFF;   /* deck: restored below */
        }
    }
    ty = (uint8_t)(((tr_y & 15u) << 1) + 1);
    mx0 = (uint8_t)((cam_x & 15u) << 1);
    for (i = 0; i < 22; ++i) {
        uint8_t mx = (uint8_t)((mx0 + i) & 31u);
        uint16_t wx = cam_x + (i >> 1);
        if (trow_t[i] == 0xFF) {
            /* deck tile for this column, from the metatile tables */
            uint8_t m1 = world_mt(wx, tr_y), m2 = world_mt(wx, tr_y + 1);
            uint16_t o1 = ((uint16_t)m1 << 2) + 2 + (i & 1), o2 = ((uint16_t)m2 << 2) + (i & 1);
            VBK_REG = VBK_ATTRIBUTES;
            set_bkg_tile_xy(mx, ty, mt_attrs[o1]);
            set_bkg_tile_xy(mx, (ty + 1) & 31u, mt_attrs[o2]);
            VBK_REG = VBK_TILES;
            set_bkg_tile_xy(mx, ty, mt_tiles[o1]);
            set_bkg_tile_xy(mx, (ty + 1) & 31u, mt_tiles[o2]);
        } else {
            uint8_t a = trow_a[i];
            if (world_mt(wx, tr_y) == MT_VIA_T_OVER) a |= 0x80u;  /* still above the walker */
            VBK_REG = VBK_ATTRIBUTES;
            set_bkg_tile_xy(mx, ty, a);
            set_bkg_tile_xy(mx, (ty + 1) & 31u, a);
            VBK_REG = VBK_TILES;
            set_bkg_tile_xy(mx, ty, trow_t[i]);
            set_bkg_tile_xy(mx, (ty + 1) & 31u, trow_t[i] + 3);
        }
    }
}
