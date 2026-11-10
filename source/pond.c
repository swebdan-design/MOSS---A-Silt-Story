/* MOSS — the pond instrument ("Sit and listen").
 *
 *  D-pad  move the cursor over the 8 x 5 pond
 *  A      drop a pebble (a note and a ripple), or place / remove a lily
 *  B      change what you're holding: Pebble, Bell, Reed, Root, Wood
 *  SELECT play / just listen (hides everything but the water)
 *  START  the pond's settings
 * The garden plays itself: stones A and B send out ripples on their own
 * cycles, and ripples that reach a lily make it sound (SILT's rule).
 */
#pragma bank 15
#include <gb/gb.h>
#include <gb/hardware.h>
#include <string.h>
#include "core.h"
#include "world.h"
#include "story.h"

uint8_t pond_answer_heard;

#define GX 16   /* pond origin on screen */
#define GY 16
enum { TOOL_PEBBLE, TOOL_BELL, TOOL_REED, TOOL_ROOT, TOOL_WOOD, TOOL_COUNT };
static const char *const tool_names[TOOL_COUNT] = {"Pebble", "Bell lily", "Reed lily", "Root lily", "Wood lily"};
static const char *const scale_names[SCALE_COUNT] = {"D minor", "C major", "Open fifths", "Suspended"};
static const char *const dens_names[DENS_COUNT] = {"Still", "Sparse", "Gentle", "Active"};
static const char *const rain_names[RAIN_COUNT] = {"Off", "Drizzle", "Rain", "Shower"};
static const char *const echo_names[ECHO_COUNT] = {"Dry", "Mist", "Deep"};

static uint8_t pmode, cx, cy, tool, listen_only, behaviours;
static uint8_t cell_tiles[11][4];   /* cell, bell, reed, root, wood, x4, stone a, b */
static uint8_t drawn[POND_CELLS];
static uint8_t last_rows[2];
static uint16_t last_drop_step;
static uint16_t minute_frames;
static uint8_t reed_timer, reed_show;
static uint8_t boy_state;            /* 0 sitting, 1 standing, 2 walking away, 3 gone */
static int16_t boy_x, boy_y;
static uint8_t stone_left;
static char line[40];

static const int8_t ring_dx[8] = {0, 7, 10, 7, 0, -7, -10, -7};
static const int8_t ring_dy[8] = {-10, -7, 0, 7, 10, 7, 0, -7};

static uint8_t tool_ok(uint8_t t) {
    if (pmode == POND_FREE) return 1;
    switch (t) {
    case TOOL_REED: return (behaviours & BEH_WIND) != 0;
    case TOOL_ROOT: return (behaviours & BEH_WATER) != 0;
    case TOOL_WOOD: return (behaviours & BEH_WOOD) != 0;
    default: return 1;
    }
}

/* ---------------------------------------------------------------- drawing */
static void load_cell_tiles(void) {
    const MapDef *m = &map_defs[MAP_PONDSCREEN];
    const uint8_t *p = m->objects + (uint16_t)m->nwarps * 6 + (uint16_t)m->nobjs * 3 + (uint16_t)m->nlistens * 5;
    uint8_t i, rec[8];
    for (i = 0; i < m->npatches && i < 11; ++i) {
        far_copy(rec, m->ob_bank, p + (uint16_t)i * 8, 8);
        memcpy(cell_tiles[rec[0] - 1], rec + 3, 4);
    }
}

static uint8_t cell_kind(uint8_t c) {
    uint8_t col = c & 7, row = c >> 3, t;
    if (garden_is_stone(col, row)) return col == 1 ? 9 : 10;
    t = pond.pads[c];
    if (!t) return 0;
    return (uint8_t)(t + (garden.flash[c] > 6 ? 4 : 0));
}

static void draw_cells(uint8_t budget) {
    uint8_t c, k;
    for (c = 0; c < POND_CELLS && budget; ++c) {
        k = cell_kind(c);
        if (drawn[c] == k) continue;
        drawn[c] = k;
        gfx_patch_cell((uint8_t)(1 + (c & 7)), (uint8_t)(1 + (c >> 3)), cell_tiles[k]);
        --budget;
    }
}

static void hud(void) {
    char *p = line;
    const char *s;
    if (listen_only || pmode == POND_FINAL) return;
    s = tool_names[tool]; while (*s) *p++ = *s++;
    *p++ = ' '; *p++ = ' ';
    s = scale_names[comp.scale]; while (*s) *p++ = *s++;
    *p++ = ' '; *p++ = ' ';
    s = dens_names[comp.density]; while (*s) *p++ = *s++;
    *p = 0;
    vwf_draw(0, 4, line);
}

static char hud_last[40];
static void hud_msg(const char *s) {
    uint8_t i = 0;
    while (s[i] && i < 39) { hud_last[i] = s[i]; ++i; }
    hud_last[i] = 0;
    vwf_draw(1, 4, hud_last);
}

static void show_hud(uint8_t on) {
    uint8_t i, row[20];
    if (!on) { win_hide(); return; }
    for (i = 0; i < 20; ++i) row[i] = (uint8_t)(i == 0 || i == 19 ? UI_BLANK : vwf_slot_tile(0) + i - 1);
    set_win_tiles(0, 0, 20, 1, row);
    for (i = 0; i < 20; ++i) row[i] = (uint8_t)(i == 0 || i == 19 ? UI_BLANK : vwf_slot_tile(1) + i - 1);
    set_win_tiles(0, 1, 20, 1, row);
    hud();
    vwf_draw(1, 4, hud_last);
    win_show(7, 128);
}

static void draw_sprites(void) {
    uint8_t i, k, used = 0, act = 0, pts, spr = 7;
    for (i = 0; i < RIPPLE_LIMIT; ++i) act += garden.rip[i].active;
    pts = act <= 4 ? 8 : 4;
    for (i = 0; i < RIPPLE_LIMIT; ++i) {
        Ripple *r = &garden.rip[i];
        if (!r->active) continue;
        for (k = 0; k < 8; k += (uint8_t)(8 / pts)) {
            int16_t x = GX + r->x + (int16_t)(((int16_t)ring_dx[k] * r->r * 13) >> 7) - 4;
            int16_t y = GY + r->y + (int16_t)(((int16_t)ring_dy[k] * r->r * 13) >> 7) - 4;
            if (spr >= 40) break;
            if (x < GX - 4 || x > GX + 124 || y < GY - 4 || y > GY + 76) { gfx_hide_small(spr++); continue; }
            gfx_small(spr++, SPR_RIPPLE, x, y - 4, 1);
            ++used;
        }
    }
    while (spr < 40) gfx_hide_small(spr++);
    /* cursor */
    if (!listen_only && pmode != POND_FINAL)
        gfx_actor(0, SPR_CURSOR, GX + cx * 16, GY + cy * 16, 0, (frame_count >> 4) & 1);
    else
        gfx_hide_actor(0);
    /* the boy on the bank */
    if (boy_state < 3) {
        uint8_t t = boy_state == 0 ? SPR_BOY_SIT : (boy_state == 1 ? SPR_BOY_UP : SPR_BOY_SIDE);
        uint8_t walk = boy_state == 2 && ((frame_count >> 3) & 1);
        gfx_actor(1, (uint8_t)(t + walk * 4), boy_x, boy_y, boy_state == 2, 0);
    } else gfx_hide_actor(1);
    if (stone_left) gfx_small(6, SPR_PEBBLE, 60, 104, 0); else gfx_hide_small(6);
    /* the reed creature, now and then, at the right bank */
    if (reed_show) gfx_actor(2, ((frame_count >> 5) & 1) ? SPR_REED_B : SPR_REED, 146, 48, 0, 1);
    else gfx_hide_actor(2);
    (void)used;
}

static void pond_frame(void) {
    frame();
    if ((frame_count & 3) == 0) garden_visual_tick();
    draw_cells(3);
    draw_sprites();
    if (++minute_frames >= 3584) { minute_frames = 0; mmem_minute(); }
    if (!reed_show && ++reed_timer == 0 && (behaviours & BEH_INSECTS) && (comp.rng & 7) == 0) reed_show = 90;
    if (reed_show) --reed_show;
}

static void pond_wait(uint16_t n) { while (n--) pond_frame(); }

/* ---------------------------------------------------------------- actions */
static void drop_pebble(void) {
    static SndEv plop;
    uint8_t row = cy;
    plop.ch = CH_ENV; plop.note = 0x24; plop.env = 0x51; plop.timbre = 0; plop.hold = 2;
    plop.prio = PRIO_PLAYER; plop.fx = 0; plop.fxarg = 0; plop.pan = PAN_C;
    snd_play(&plop, 0);
    compose_player_note(LILY_BELL, row, cx, 1);
    garden_spawn((int16_t)(cx * 16 + 8), (int16_t)(cy * 16 + 8), 2);
    /* The pond's answer: low row, then the second row - the bell's interval. */
    last_rows[0] = last_rows[1];
    last_rows[1] = row;
    if (pmode == POND_STORY && game.day == 4 && (game.found & BEH_MEMORY) && !pond_answer_heard &&
        last_rows[0] == 4 && last_rows[1] == 1 && (uint16_t)(comp.step - last_drop_step) < 48) {
        uint8_t k;
        pond_wait(50);
        compose_hush(48);
        reed_show = 255;
        pond_wait(30);
        {
            static SndEv a;
            a.ch = CH_LIVING; a.env = 0x83; a.timbre = 2; a.hold = 30; a.prio = PRIO_MOTIF;
            a.fx = FX_VIB; a.fxarg = 1; a.pan = PAN_R;
            a.note = compose_scale_midi(13); snd_play(&a, 0);
            a.note = compose_scale_midi(10); snd_play(&a, 30);
        }
        pond_wait(70);
        compose_motif(MOTIF_ANSWER, 0);
        for (k = 0; k < 6; ++k) garden_spawn((int16_t)(16 + k * 20), (int16_t)(8 + (k & 1) * 56), 2);
        pond_wait(120);
        pond_answer_heard = 1;
        hud_msg("The pond answered.");
    }
    last_drop_step = comp.step;
}

static void toggle_lily(void) {
    uint8_t c = (uint8_t)(cy * POND_COLS + cx), type = tool; /* TOOL_BELL==LILY_BELL ... */
    if (garden_is_stone(cx, cy)) { hud_msg("A stone. It was here first."); return; }
    if (pond.pads[c] == type) { pond.pads[c] = LILY_NONE; hud_msg("Lifted out."); }
    else {
        if (pond.pads[c] == LILY_NONE && garden_count() >= 20) { hud_msg("That's enough lilies for one pond."); return; }
        pond.pads[c] = type;
        compose_player_note(type, cy, cx, 1);
        hud_msg("");
    }
}

/* ---------------------------------------------------------------- menu */
#define ITEMS_MAX 320
static char items[ITEMS_MAX];
static void add(char **p, const char *a, const char *b) {
    char *end = items + ITEMS_MAX - 2;
    while (*a && *p < end) *(*p)++ = *a++;
    if (b) {
        if (*p < end) *(*p)++ = ':';
        if (*p < end) *(*p)++ = ' ';
        while (*b && *p < end) *(*p)++ = *b++;
    }
    *(*p)++ = '|';
}

enum { M_LEAVE, M_SCALE, M_DENS, M_TEMPO, M_DRIFT, M_RAIN, M_ECHO, M_WATER, M_INSECTS, M_HOLD,
       M_PRESET, M_SAVE, M_LOAD, M_CLEAR, M_COUNT };

static uint8_t pond_menu(void) {
    uint8_t ids[M_COUNT], n, sel = 0, r;
    char tempo[4];
    for (;;) {
        char *p = items;
        n = 0;
        num_to_str(tempo, comp.bpm);
        add(&p, pmode == POND_FREE ? "Title screen" : "Stand up", 0); ids[n++] = M_LEAVE;
        add(&p, "Scale", scale_names[pond.scale]); ids[n++] = M_SCALE;
        add(&p, "Density", dens_names[pond.density]); ids[n++] = M_DENS;
        add(&p, "Tempo", tempo); ids[n++] = M_TEMPO;
        if (behaviours & BEH_WIND) { add(&p, "Wind", pond.drift ? "Drift" : "Calm"); ids[n++] = M_DRIFT; }
        if (behaviours & BEH_RAIN) { add(&p, "Rain", rain_names[pond.rain]); ids[n++] = M_RAIN; }
        if (behaviours & BEH_ECHO) { add(&p, "Echo", echo_names[pond.echo]); ids[n++] = M_ECHO; }
        if (behaviours & BEH_WATER) { add(&p, "Water", pond.water ? "On" : "Off"); ids[n++] = M_WATER; }
        if (behaviours & BEH_INSECTS) { add(&p, "Insects", pond.insects ? "On" : "Off"); ids[n++] = M_INSECTS; }
        add(&p, "Hold", pond.hold ? "On" : "Off"); ids[n++] = M_HOLD;
        if (pmode == POND_FREE) {
            {
                static char pn[4];
                pn[0] = (char)('1' + (game.preset & 7)); pn[1] = '/'; pn[2] = '8'; pn[3] = 0;
                add(&p, "Preset", pn); ids[n++] = M_PRESET;
            }
            add(&p, "Keep my pond", 0); ids[n++] = M_SAVE;
            if (game.my_pond_saved) { add(&p, "Load my pond", 0); ids[n++] = M_LOAD; }
        }
        add(&p, "Clear lilies", 0); ids[n++] = M_CLEAR;
        p[-1] = 0;
        r = menu(items, n, sel, 13, 1);
        if (r == 0xFF) break;
        sel = r;
        switch (ids[r]) {
        case M_LEAVE:
            if (!menu_lr) { win_hide(); return 1; }
            break;
        case M_SCALE:
            pond.scale = (uint8_t)((pond.scale + (menu_lr < 0 ? SCALE_COUNT - 1 : 1)) % SCALE_COUNT);
            mmem_choose(MM_SCALE, pond.scale);
            break;
        case M_DENS:
            pond.density = (uint8_t)((pond.density + (menu_lr < 0 ? DENS_COUNT - 1 : 1)) % DENS_COUNT);
            if (pond.density == DENS_STILL && !(behaviours & BEH_STILL)) pond.density = menu_lr < 0 ? DENS_ACTIVE : DENS_SPARSE;
            break;
        case M_TEMPO: {
            int16_t b = (int16_t)pond.bpm + (menu_lr < 0 ? -4 : 4);
            if (b < 52) b = 52;
            if (b > 120) b = 120;
            pond.bpm = (uint8_t)b;
        } break;
        case M_DRIFT: pond.drift ^= 1; break;
        case M_RAIN: pond.rain = (uint8_t)((pond.rain + (menu_lr < 0 ? RAIN_COUNT - 1 : 1)) % RAIN_COUNT); break;
        case M_ECHO: pond.echo = (uint8_t)((pond.echo + (menu_lr < 0 ? ECHO_COUNT - 1 : 1)) % ECHO_COUNT); break;
        case M_WATER: pond.water ^= 1; if (!pond.water) snd_drone_off(); break;
        case M_INSECTS: pond.insects ^= 1; break;
        case M_HOLD: pond.hold ^= 1; break;
        case M_PRESET:
            game.preset = (uint8_t)((game.preset + (menu_lr < 0 ? 7 : 1)) & 7);
            snd_drone_off();
            garden_preset(game.preset);
            memset(drawn, 0xFF, sizeof(drawn));
            {
                static char nm[24];
                garden_preset_name_copy(game.preset, nm);
                hud_msg(nm);
            }
            break;
        case M_SAVE:
            if (!menu_lr) { memcpy(&game.my_pond, &pond, sizeof(pond)); game.my_pond_saved = 1; save_write(); hud_msg("Kept. It will be here next time."); }
            break;
        case M_LOAD:
            if (!menu_lr) { snd_drone_off(); memcpy(&pond, &game.my_pond, sizeof(pond)); garden_reset(); memset(drawn, 0xFF, sizeof(drawn)); }
            break;
        case M_CLEAR:
            if (!menu_lr) { memset(pond.pads, 0, sizeof(pond.pads)); memset(drawn, 0xFF, sizeof(drawn)); }
            break;
        }
        compose_apply_pond();
        draw_cells(40);
    }
    win_hide();
    show_hud(1);
    return 0;
}

/* ---------------------------------------------------------------- setup */
static void pond_enter(void) {
    fade_out();
    LCD_OFF;
    win_hide();
    gfx_clear_sprites();
    gfx_load_map(MAP_PONDSCREEN);
    SCX_REG = 0; SCY_REG = 0;
    {
        extern uint8_t scroll_x, scroll_y;
        scroll_x = 0; scroll_y = 0;
    }
    load_cell_tiles();
    memset(drawn, 0xFF, sizeof(drawn));
    draw_cells(40);
    /* HUD window: two text rows */
    {
        uint8_t i, row[20];
        for (i = 0; i < 20; ++i) row[i] = (uint8_t)(i == 0 || i == 19 ? UI_BLANK : vwf_slot_tile(0) + i - 1);
        set_win_tiles(0, 0, 20, 1, row);
        for (i = 0; i < 20; ++i) row[i] = (uint8_t)(i == 0 || i == 19 ? UI_BLANK : vwf_slot_tile(1) + i - 1);
        set_win_tiles(0, 1, 20, 1, row);
    }
    vwf_line_clear(0);
    vwf_line_clear(1);
    boy_x = 72; boy_y = 100; boy_state = 0;
    bgp_normal = 0xE4;
    DISPLAY_ON;
    fade_in();
}

static void build_final_pond(void) {
    /* The last pond: the player's own garden, coloured by everything the
       pond has learned. A fresh game's pond cannot sound like this. */
    pond.scale = mmem_favourite_scale();
    pond.density = mmem_preferred_density();
    if (pond.density == DENS_ACTIVE) pond.density = DENS_GENTLE;
    pond.water = 1; pond.insects = 1; pond.hold = 0;
    pond.drift = (game.found & BEH_WIND) ? 1 : 0;
    if ((game.found & BEH_ECHO) && pond.echo == ECHO_DRY) pond.echo = ECHO_MIST;
    if ((game.found & BEH_RAIN) && pond.rain == RAIN_OFF) pond.rain = RAIN_DRIZZLE;
    if (garden_count() < 3) garden_preset(6);
    pond.bpm = pond.bpm ? pond.bpm : 72;
}

static void final_sequence(void) {
    uint8_t i;
    boy_state = 0;
    pond_wait(240);
    compose_motif(MOTIF_POND, 0);
    pond_wait(600);
    /* the notebook's last page */
    page_open();
    page_center(2, "Day five. Early.");
    page_wrap(5, 6, "I'm leaving a stone by the pond. Not for the pond. The pond doesn't need anything. It's so that something of mine gets to stay.");
    page_center(14, "- the last page -");
    wait_frames(255); wait_frames(200);
    page_close();
    memset(drawn, 0xFF, sizeof(drawn));
    for (i = 0; i < 60; ++i) pond_frame();
    /* he places the stone, stands, and goes */
    stone_left = 1;
    sfx_step_stone(62);
    pond_wait(120);
    boy_state = 1;
    pond_wait(90);
    boy_state = 2;
    while (boy_x > -20) { boy_x -= 1; pond_frame(); pond_frame(); }
    boy_state = 3;
    /* the camera stays with the water */
    pond_wait(1400);
    reed_show = 220;
    pond_wait(900);
    page_open();
    gfx_set_palette(0xE4);
    page_center(6, "I thought I had found");
    page_center(7, "the pond.");
    for (i = 0; i < 4; ++i) wait_frames(150);
    page_center(10, "But it was always here.");
    for (i = 0; i < 5; ++i) wait_frames(150);
    page_close();
    memset(drawn, 0xFF, sizeof(drawn));
    pond_wait(900);
}

void pond_run(uint8_t m) BANKED {
    uint8_t rep = 0;
    pmode = m;
    pond_answer_heard = 0;
    listen_only = 0;
    stone_left = (pmode == POND_FREE && game.postgame) || FLAG(F_STONE_LEFT);
    reed_show = 0;
    cx = 3; cy = 2; tool = TOOL_PEBBLE;
    last_rows[0] = last_rows[1] = 0xFF;
    behaviours = pmode == POND_STORY ? game.found : BEH_ALL;
    if (pmode != POND_STORY) memcpy(&mmem, &game.mmem, sizeof(mmem));
    if (pmode == POND_FREE && !game.my_pond_saved && garden_count() < 3) garden_preset(game.preset);
    if (pmode == POND_FINAL) build_final_pond();
    garden_reset();
    pond_enter();
    compose_scene(pmode == POND_FINAL ? SCENE_ENDING : SCENE_POND, behaviours);
    if (pmode == POND_STORY && game.day == 3) comp.weather = RAIN_DRIZZLE;
    if (pmode == POND_FINAL) {
        final_sequence();
        memcpy(&game.pond, &pond, sizeof(pond));
        memcpy(&game.mmem, &mmem, sizeof(mmem));
        return;
    }
    show_hud(1);
    hud();
    if (pmode == POND_STORY && !FLAG(F_POND_TUTORIAL)) {
        SET(F_POND_TUTORIAL);
        say("(The pond is an instrument. The stones send out ripples by themselves; a ripple that reaches a lily makes it sound.)");
        say("(+ pad: move. A: drop a pebble, or place a lily. B: change what you hold. SELECT: just listen. START: the pond's settings.)");
        say("(You don't have to do anything. Listening counts.)");
        say_close();
        show_hud(1);
        hud();
        hud_msg("");
    }
    if (pmode == POND_FREE) hud_msg("Return to the pond.");
    for (;;) {
        pond_frame();
        if (listen_only) {
            if (pressed & (J_SELECT | J_START | J_A | J_B)) { listen_only = 0; show_hud(1); hud(); }
            continue;
        }
        if (pressed & J_START) { if (pond_menu()) break; hud(); continue; }
        if (pressed & J_SELECT) { listen_only = 1; show_hud(0); continue; }
        if (pressed & J_B) {
            do { tool = (uint8_t)((tool + 1) % TOOL_COUNT); } while (!tool_ok(tool));
            sfx_blip(); hud(); hud_msg(""); continue;
        }
        if (pressed & J_A) { if (tool == TOOL_PEBBLE) drop_pebble(); else toggle_lily(); continue; }
        if (keys & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            if ((pressed & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) || ++rep > 12) {
                if (!(pressed & (J_UP | J_DOWN | J_LEFT | J_RIGHT))) rep = 8;
                else rep = 0;
                if ((keys & J_UP) && cy) --cy;
                if ((keys & J_DOWN) && cy < POND_ROWS - 1) ++cy;
                if ((keys & J_LEFT) && cx) --cx;
                if ((keys & J_RIGHT) && cx < POND_COLS - 1) ++cx;
            }
        } else rep = 0;
    }
    memcpy(&game.pond, &pond, sizeof(pond));
    memcpy(&game.mmem, &mmem, sizeof(mmem));
    win_hide();
    gfx_clear_sprites();
}
