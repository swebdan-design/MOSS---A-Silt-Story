/* MOSS — overworld engine: maps, movement, camera, people, listening. */
#pragma bank 2
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

extern uint8_t scroll_x, scroll_y;

Npc npcs[NPC_SLOTS];
uint8_t px, py, pdir;
uint8_t world_exit;
uint8_t listening;
uint8_t coll[256];
uint8_t map_w, map_h;

static uint8_t warps[12][6], nwarps;
static uint8_t objs[24][3], nobjs;
static uint8_t listens[8][5], nlistens;
static uint8_t patches[24][8], npatches;

static int8_t pox, poy;        /* player pixel offset while moving */
static uint8_t moving, mdir, step_anim, hold, player_hidden;
static int16_t cam_x, cam_y;
static uint8_t cam_locked;
static uint8_t listen_armed, listen_timer;
static uint8_t wander_timer;

/* -------------------------------------------------------------- particles */
#define PMAX 14
#define PSPRITE 18
typedef struct { int16_t x, y; int8_t vx, vy; uint8_t life, tile, kind, pal; } Part;
static Part parts[PMAX];
static uint8_t rain_on, fireflies_on;
enum { P_NOTE = 1, P_DROP, P_FLY, P_SPARK, P_RIPPLE };

static const uint8_t who_tiles[WHO_COUNT][3] = {
    {SPR_BOY_DOWN, SPR_BOY_UP, SPR_BOY_SIDE},
    {SPR_MAE_DOWN, SPR_MAE_UP, SPR_MAE_SIDE},
    {SPR_IRIS_DOWN, SPR_IRIS_UP, SPR_IRIS_SIDE},
    {SPR_VALE_DOWN, SPR_VALE_UP, SPR_VALE_SIDE},
    {SPR_HOLLIS_DOWN, SPR_HOLLIS_UP, SPR_HOLLIS_SIDE},
    {SPR_ADA_DOWN, SPR_ADA_UP, SPR_ADA_SIDE},
    {SPR_CAT_SIT, SPR_CAT_SIT, SPR_CAT_SIDE},
    {SPR_REED, SPR_REED_B, SPR_REED},
};

static const int8_t dx_of[4] = {0, 0, -1, 1};
static const int8_t dy_of[4] = {1, -1, 0, 0};

/* -------------------------------------------------------------- helpers */
static uint8_t cell(uint8_t x, uint8_t y) {
    if (x >= map_w || y >= map_h) return C_SOLID;
    return coll[(uint16_t)y * map_w + x];
}

uint8_t npc_at(uint8_t x, uint8_t y) BANKED {
    uint8_t i;
    for (i = 0; i < NPC_SLOTS; ++i)
        if (npcs[i].who && !npcs[i].hidden && npcs[i].x == x && npcs[i].y == y) return npcs[i].who;
    return 0;
}

static Npc *find(uint8_t who) {
    uint8_t i;
    for (i = 0; i < NPC_SLOTS; ++i) if (npcs[i].who == who) return &npcs[i];
    return 0;
}

static uint8_t passable(uint8_t x, uint8_t y) {
    if (x >= map_w || y >= map_h) return 0;
    if (cell(x, y) & C_SOLID) return 0;
    if (x == px && y == py) return 0;
    return npc_at(x, y) == 0;
}

uint8_t world_scene(void) BANKED { return map_defs[cur_map].scene; }

/* -------------------------------------------------------------- camera */
void cam_follow(void) BANKED {
    int16_t mx = (int16_t)map_w * 16 - 160, my = (int16_t)map_h * 16 - 144;
    cam_x = (int16_t)px * 16 + pox - 72;
    cam_y = (int16_t)py * 16 + poy - 64;
    if (cam_x > mx) cam_x = mx;
    if (cam_y > my) cam_y = my;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    scroll_x = (uint8_t)cam_x;
    scroll_y = (uint8_t)cam_y;
}

void cam_set(int16_t x, int16_t y) BANKED {
    cam_locked = 1;
    cam_x = x; cam_y = y;
    scroll_x = (uint8_t)x; scroll_y = (uint8_t)y;
}

/* -------------------------------------------------------------- drawing */
static void draw_actor(uint8_t slot, uint8_t who, uint8_t dir, int16_t wx, int16_t wy, uint8_t walk) {
    uint8_t tile, flip = 0, pal = 0;
    const uint8_t *t = who_tiles[who];
    if (dir == DIR_DOWN) tile = t[0];
    else if (dir == DIR_UP) tile = t[1];
    else { tile = t[2]; flip = dir == DIR_LEFT; }
    if (who == 0 && walk) tile += 4;                     /* boy walk frames follow */
    if (who == WHO_CAT && walk && dir >= DIR_LEFT) tile = SPR_CAT_SIDE_B;
    if (who == WHO_CAT && dir >= DIR_LEFT) flip = dir == DIR_RIGHT; /* cat art faces left */
    if (who == WHO_REED) { pal = 1; if ((frame_count >> 5) & 1) tile = SPR_REED_B; }
    gfx_actor(slot, tile, wx - cam_x, wy - cam_y - 2, flip, pal);
}

static void draw_parts(void) {
    uint8_t i;
    for (i = 0; i < PMAX; ++i) {
        Part *p = &parts[i];
        if (!p->life) { gfx_hide_small(PSPRITE + i); continue; }
        gfx_small(PSPRITE + i, p->tile, p->kind == P_DROP ? p->x : p->x - cam_x,
                  p->kind == P_DROP ? p->y : p->y - cam_y, p->pal);
    }
}

static void update_parts(void) {
    uint8_t i;
    for (i = 0; i < PMAX; ++i) {
        Part *p = &parts[i];
        if (!p->life) {
            if (rain_on && i < 8) {
                p->kind = P_DROP; p->tile = SPR_DROP; p->pal = 1;
                p->x = (int16_t)(((uint16_t)frame_count * 37u + i * 53u) % 168u) - 4;
                p->y = -8 - (int16_t)(i * 9); p->vx = -1; p->vy = 4; p->life = 40;
            } else if (fireflies_on && i >= 8) {
                p->kind = P_FLY; p->tile = SPR_FIREFLY; p->pal = 1;
                p->x = cam_x + 30 + (int16_t)(((uint16_t)frame_count * 13u + i * 71u) % 100u);
                p->y = cam_y + 30 + (int16_t)(((uint16_t)frame_count * 7u + i * 29u) % 80u);
                p->vx = 0; p->vy = 0; p->life = 120;
            }
            continue;
        }
        --p->life;
        if (p->kind == P_FLY) {
            if ((frame_count & 7) == i % 8) { p->x += (int8_t)((frame_count >> 3) % 3) - 1; p->y -= 1; }
            if (p->life < 20 && (frame_count & 2)) { gfx_hide_small(PSPRITE + i); }
        } else if (p->kind == P_RIPPLE) {
            if ((p->life & 7) == 0) p->tile = p->tile == SPR_RIPPLE ? SPR_LISTEN : SPR_RIPPLE;
        } else if ((frame_count & 1) || p->kind == P_DROP) {
            p->x += p->vx; p->y += p->vy;
        }
    }
}

static void draw_world(void) {
    uint8_t i;
    Npc *n;
    if (!cam_locked) cam_follow();
    if (!player_hidden)
        draw_actor(0, 0, pdir, (int16_t)px * 16 + pox, (int16_t)py * 16 + poy, moving && (step_anim & 4));
    else
        gfx_hide_actor(0);
    for (i = 0; i < NPC_SLOTS; ++i) {
        n = &npcs[i];
        if (!n->who || n->hidden) { gfx_hide_actor(1 + i); continue; }
        draw_actor(1 + i, n->who, n->dir, (int16_t)n->x * 16 + n->ox, (int16_t)n->y * 16 + n->oy,
                   (n->ox | n->oy) != 0 || (n->who == WHO_CAT && n->wander && (frame_count & 16)));
    }
    if (listening && !player_hidden)
        gfx_small(PSPRITE - 1, SPR_LISTEN, (int16_t)px * 16 + 4 - cam_x, (int16_t)py * 16 - 14 - cam_y +
                  (int16_t)((frame_count >> 4) & 1), 1);
    else
        gfx_hide_small(PSPRITE - 1);
    update_parts();
    draw_parts();
}

static void cat_wander(void) {
    Npc *c = find(WHO_CAT);
    uint8_t d, nx, ny;
    if (!c || !c->wander || c->hidden) return;
    if (++wander_timer < 70) return;
    wander_timer = 0;
    d = (uint8_t)((frame_count ^ (frame_count >> 3)) & 3);
    nx = c->x + dx_of[d]; ny = c->y + dy_of[d];
    if (d >= DIR_LEFT) c->dir = d;
    if (passable(nx, ny) && !(cell(nx, ny) & C_WATER)) {
        uint8_t k;
        for (k = 0; k < 8; ++k) {
            c->ox += dx_of[d] * 2; c->oy += dy_of[d] * 2;
            draw_world(); frame();
        }
        c->x = nx; c->y = ny; c->ox = 0; c->oy = 0;
    }
}

void world_wait(uint8_t frames) BANKED {
    while (frames--) { draw_world(); frame(); }
}

void world_redraw(void) BANKED { draw_world(); }

/* -------------------------------------------------------------- npcs */
void npc_place(uint8_t who, uint8_t x, uint8_t y, uint8_t dir) BANKED {
    Npc *n = find(who);
    uint8_t i;
    if (!n) for (i = 0; i < NPC_SLOTS; ++i) if (!npcs[i].who) { n = &npcs[i]; break; }
    if (!n) return;
    n->who = who; n->x = x; n->y = y; n->dir = dir; n->ox = n->oy = 0; n->hidden = 0; n->wander = 0;
}

void npc_remove(uint8_t who) BANKED {
    Npc *n = find(who);
    if (n) n->who = 0;
}

void npc_face(uint8_t who, uint8_t dir) BANKED {
    Npc *n = find(who);
    if (n) n->dir = dir;
}

void npc_wander(uint8_t who, uint8_t on) BANKED {
    Npc *n = find(who);
    if (n) n->wander = on;
}

void face_player(uint8_t who) BANKED {
    Npc *n = find(who);
    if (!n) return;
    if (n->x < px) n->dir = DIR_RIGHT;
    else if (n->x > px) n->dir = DIR_LEFT;
    else if (n->y < py) n->dir = DIR_DOWN;
    else n->dir = DIR_UP;
}

void npc_walk(uint8_t who, uint8_t dir, uint8_t tiles) BANKED {
    Npc *n = find(who);
    uint8_t k;
    if (!n) return;
    n->dir = dir;
    while (tiles--) {
        for (k = 0; k < 16; ++k) {
            n->ox += dx_of[dir]; n->oy += dy_of[dir];
            draw_world(); frame();
        }
        n->x += dx_of[dir]; n->y += dy_of[dir]; n->ox = n->oy = 0;
    }
}

void player_face(uint8_t dir) BANKED { pdir = dir; draw_world(); }
void player_hide(uint8_t hide) BANKED { player_hidden = hide; draw_world(); }

void player_walk(uint8_t dir, uint8_t tiles) BANKED {
    uint8_t k;
    pdir = dir;
    while (tiles--) {
        for (k = 0; k < 8; ++k) {
            pox += dx_of[dir] * 2; poy += dy_of[dir] * 2; ++step_anim;
            moving = 1;
            draw_world(); frame();
        }
        px += dx_of[dir]; py += dy_of[dir]; pox = poy = 0;
    }
    moving = 0;
    draw_world();
}

/* -------------------------------------------------------------- effects */
static Part *free_part(uint8_t from) {
    uint8_t i;
    for (i = from; i < PMAX; ++i) if (!parts[i].life) return &parts[i];
    return 0;
}

void fx_notes(uint8_t from_dir, uint8_t count) BANKED {
    /* notes drift across the screen from the direction of a sound */
    while (count--) {
        Part *p = free_part(8);
        if (!p) return;
        p->kind = P_NOTE; p->tile = SPR_NOTE; p->pal = 0; p->life = 90;
        p->x = (int16_t)px * 16 + 4; p->y = (int16_t)py * 16 - 4 - (int16_t)(count * 6);
        p->x += -dx_of[from_dir] * (40 + count * 10);
        p->y += -dy_of[from_dir] * (30 + count * 8);
        p->vx = dx_of[from_dir]; p->vy = dy_of[from_dir];
        if (!p->vy) p->vy = 0;
    }
}

void fx_sparkle(uint8_t x, uint8_t y, uint8_t count) BANKED {
    while (count--) {
        Part *p = free_part(8);
        if (!p) return;
        p->kind = P_SPARK; p->tile = SPR_FIREFLY; p->pal = 1; p->life = (uint8_t)(50 + count * 12);
        p->x = (int16_t)x * 16 + 4 + (int16_t)(count * 5) - 8; p->y = (int16_t)y * 16 + 4;
        p->vx = 0; p->vy = -1;
    }
}

void fx_ripples(uint8_t x, uint8_t y, uint8_t size) BANKED {
    uint8_t i;
    for (i = 0; i < size; ++i) {
        Part *p = free_part(8);
        if (!p) return;
        p->kind = P_RIPPLE; p->tile = SPR_RIPPLE; p->pal = 1; p->life = (uint8_t)(30 + size * 10);
        p->x = (int16_t)x * 16 + 4 + (int16_t)((i & 1) ? i * 3 : -(int16_t)i * 3);
        p->y = (int16_t)y * 16 + (int16_t)((i & 2) ? i * 2 : -(int16_t)i * 2);
        p->vx = 0; p->vy = 0;
    }
}

void fx_rain(uint8_t on) BANKED { rain_on = on; }
void fx_fireflies(uint8_t on) BANKED { fireflies_on = on; }

void fx_clear(void) BANKED {
    uint8_t i;
    for (i = 0; i < PMAX; ++i) { parts[i].life = 0; gfx_hide_small(PSPRITE + i); }
}

/* -------------------------------------------------------------- map loading */
static void read_objects(void) {
    const MapDef *m = &map_defs[cur_map];
    uint8_t buf[6], i;
    const uint8_t *p = m->objects;
    nwarps = m->nwarps > 12 ? 12 : m->nwarps;
    for (i = 0; i < m->nwarps; ++i, p += 6) if (i < 12) far_copy(warps[i], m->ob_bank, p, 6);
    nobjs = m->nobjs > 24 ? 24 : m->nobjs;
    for (i = 0; i < m->nobjs; ++i, p += 3) if (i < 24) far_copy(objs[i], m->ob_bank, p, 3);
    nlistens = m->nlistens > 8 ? 8 : m->nlistens;
    for (i = 0; i < m->nlistens; ++i, p += 5) if (i < 8) far_copy(listens[i], m->ob_bank, p, 5);
    npatches = m->npatches > 24 ? 24 : m->npatches;
    for (i = 0; i < m->npatches; ++i, p += 8) if (i < 24) far_copy(patches[i], m->ob_bank, p, 8);
    (void)buf;
}

void world_patch(uint8_t group) BANKED {
    uint8_t i;
    for (i = 0; i < npatches; ++i) {
        if (patches[i][0] != group) continue;
        gfx_patch_cell(patches[i][1], patches[i][2], &patches[i][3]);
        coll[(uint16_t)patches[i][2] * map_w + patches[i][1]] = patches[i][7];
    }
}

void world_enter(uint8_t map, uint8_t x, uint8_t y, uint8_t dir) BANKED {
    const MapDef *m = &map_defs[map];
    uint8_t i;
    say_close();
    fade_out();
    LCD_OFF;
    win_hide();
    gfx_clear_sprites();
    fx_clear();
    rain_on = 0; fireflies_on = 0;
    memset(npcs, 0, sizeof(npcs));
    gfx_load_map(map);
    map_w = m->w; map_h = m->h;
    far_copy(coll, m->mp_bank, m->map + (uint16_t)m->w * 4u * m->h, (uint16_t)m->w * m->h);
    read_objects();
    px = x; py = y; pdir = dir; pox = poy = 0; moving = 0; player_hidden = 0; cam_locked = 0;
    listening = 0; listen_armed = 1; listen_timer = 0;
    game.map = map; game.x = x; game.y = y; game.dir = dir;
    story_setup(map);
    compose_scene(m->scene, story_scene_behaviours());
    cam_follow();
    for (i = 0; i < 2; ++i) draw_world();
    DISPLAY_ON;
    fade_in();
    story_after_enter(map);
}

/* -------------------------------------------------------------- gameplay */
static void interact(void) {
    uint8_t tx = px + dx_of[pdir], ty = py + dy_of[pdir], who, i;
    who = npc_at(tx, ty);
    if (who) {
        story_talk(who);
        say_close();
        return;
    }
    for (i = 0; i < nobjs; ++i) {
        if (objs[i][0] == tx && objs[i][1] == ty) {
            story_object(cur_map, objs[i][2]);
            say_close();
            return;
        }
    }
    if (cur_map == MAP_POND && (cell(tx, ty) & C_WATER)) {
        story_water();
        say_close();
    }
}

static uint8_t listen_threshold(void) {
    return game.settings.listen_time ? 150 : 80;
}

static void check_listen(void) {
    uint8_t i;
    if (moving) { listening = 0; return; }
    if (idle_frames < listen_threshold()) {
        if (listening) { listening = 0; comp.quiet = 0; }
        if (idle_frames < 2) { listen_armed = 1; listen_timer = 0; }
        return;
    }
    if (!listening) { listening = 1; comp.quiet = 1; listen_timer = 0; }
    if (!listen_armed) return;
    if (++listen_timer < 50) return;
    listen_armed = 0;
    for (i = 0; i < nlistens; ++i) {
        if (px >= listens[i][0] && px <= listens[i][2] && py >= listens[i][1] && py <= listens[i][3]) {
            story_listen(cur_map, listens[i][4]);
            say_close();
            idle_frames = 0;
            return;
        }
    }
    story_listen(cur_map, 0);
    say_close();
}

static void step_done(void) {
    uint8_t i;
    game.x = px; game.y = py; game.dir = pdir;
    for (i = 0; i < nwarps; ++i) {
        if (warps[i][0] == px && warps[i][1] == py) {
            if (story_block_warp(cur_map, px, py)) {
                /* step back where we came from */
                player_walk(pdir ^ 1, 1);
                return;
            }
            world_enter(warps[i][2], warps[i][3], warps[i][4], warps[i][5]);
            return;
        }
    }
    if (cell(px, py) & C_TALL) {
        static const SndEv rustle = {CH_ENV, 0x45, 0x21, 0, 6, PRIO_AMBIENT, 0, 0, PAN_C};
        snd_play(&rustle, 0);
    }
    story_step(cur_map, px, py);
}

void world_run(void) BANKED {
    uint8_t d;
    world_exit = WX_NONE;
    while (world_exit == WX_NONE) {
        draw_world();
        frame();
        if (moving) {
            pox += dx_of[mdir] * 2; poy += dy_of[mdir] * 2; ++step_anim;
            if (--moving == 0) {
                px += dx_of[mdir]; py += dy_of[mdir]; pox = poy = 0;
                step_done();
            }
            continue;
        }
        cat_wander();
        if (pressed & J_START) { pause_menu(); continue; }
        if (pressed & J_SELECT) { notebook_open(0xFF); continue; }
        if (pressed & J_A) { interact(); continue; }
        d = 0xFF;
        if (keys & J_DOWN) d = DIR_DOWN;
        else if (keys & J_UP) d = DIR_UP;
        else if (keys & J_LEFT) d = DIR_LEFT;
        else if (keys & J_RIGHT) d = DIR_RIGHT;
        if (d != 0xFF) {
            if (d != pdir && (pressed & (J_DOWN | J_UP | J_LEFT | J_RIGHT))) { pdir = d; hold = 0; continue; }
            if (hold < 3) { ++hold; if (d != pdir) pdir = d; continue; }
            pdir = d;
            if (passable(px + dx_of[d], py + dy_of[d])) { mdir = d; moving = 8; listening = 0; comp.quiet = 0; }
        } else {
            hold = 3;
        }
        check_listen();
    }
}
